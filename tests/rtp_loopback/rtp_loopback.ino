// End-to-end test for RtpSender + RtpReceiver over loopback UDP.
//
// Covers:
//   * L16 mono and stereo PCM round-trip (bit-exact);
//   * Encoded (PCMU-shaped) payload round-trip;
//   * Sequence-number continuity across multiple packets;
//   * Marker bit set on the first packet only;
//   * SSRC stability across packets.
//
// HOST PROFILE ONLY. The arduino-esp32 build of lwIP does not enable
// the loopback interface (LWIP_HAVE_LOOPIF), so the same-process
// 127.0.0.1 pattern used here cannot run on ESP32 hardware. The
// on-wire RTP path is exercised on real hardware by
// rtp_python_loopback/. See tests/README.md "ESP32 vs host".

#include <WiFiUdp.h>
#include <PCMFlowUDP.h>

static int g_pass = 0;
static int g_total = 0;

#define EXPECT_TRUE(name, cond) do { \
    ++g_total; \
    if (cond) { ++g_pass; Serial.print("PASS "); Serial.println(name); } \
    else { Serial.print("FAIL "); Serial.print(name); Serial.println(" cond"); } \
} while (0)

#define EXPECT_EQ(name, expected, actual) do { \
    ++g_total; \
    const long _e = (long)(expected); \
    const long _a = (long)(actual); \
    if (_e == _a) { ++g_pass; Serial.print("PASS "); Serial.println(name); } \
    else { \
        Serial.print("FAIL "); Serial.print(name); \
        Serial.print(" expected="); Serial.print(_e); \
        Serial.print(" actual=");   Serial.println(_a); \
    } \
} while (0)

static constexpr uint16_t kPort = 47300;

template <typename Recv>
static bool pumpUntilProgress(Recv &r)
{
    for (int i = 0; i < 100; ++i)
    {
        if (r.poll())
            return true;
        delay(5);
    }
    return false;
}

static void test_l16_mono_round_trip()
{
    WiFiUDP txUdp, rxUdp;
    EXPECT_TRUE("l16m/tx-begin", txUdp.begin(0) == 1);

    RtpReceiver rx(rxUdp);
    RtpSender tx(txUdp);

    EXPECT_TRUE("l16m/rx-begin", rx.begin(kPort));
    EXPECT_TRUE("l16m/tx-init", tx.begin(IPAddress(127, 0, 0, 1), kPort, 0xABCDEF01));
    EXPECT_EQ("l16m/ssrc", (long)0xABCDEF01L, (long)tx.ssrc());
    EXPECT_TRUE("l16m/setFormat", tx.setFormat({8000, 1, 16}));
    EXPECT_EQ("l16m/pt", (long)pcmflowudp::RtpPayloadType::L16Mono, (long)tx.payloadType());

    // 160 samples at 8 kHz = 20 ms, the natural per-packet size.
    EXPECT_EQ("l16m/spp", 160L, (long)tx.samplesPerPacket());

    static int16_t input[160];
    for (size_t i = 0; i < 160; ++i)
        input[i] = static_cast<int16_t>(i * 200 - 16000);

    EXPECT_EQ("l16m/writeFrames", 160L, (long)tx.writeFrames(input, 160));

    EXPECT_TRUE("l16m/rx-poll", pumpUntilProgress(rx));
    EXPECT_TRUE("l16m/rx-isPcm", rx.isPcm());
    EXPECT_EQ("l16m/rx-pt", (long)pcmflowudp::RtpPayloadType::L16Mono, (long)rx.payloadType());
    EXPECT_EQ("l16m/rx-ssrc", (long)0xABCDEF01L, (long)rx.ssrc());
    EXPECT_TRUE("l16m/rx-marker", rx.marker()); // first packet → marker set
    EXPECT_EQ("l16m/rx-channels", 1L, (long)rx.format().channels);

    static int16_t output[160] = {0};
    const size_t got = rx.readFrames(output, 160);
    EXPECT_EQ("l16m/readFrames", 160L, (long)got);

    int mismatches = 0;
    for (size_t i = 0; i < 160; ++i)
        if (output[i] != input[i])
            ++mismatches;
    EXPECT_EQ("l16m/sample-mismatches", 0L, (long)mismatches);

    rx.end();
    tx.end();
}

static void test_l16_stereo_round_trip()
{
    WiFiUDP txUdp, rxUdp;
    EXPECT_TRUE("l16s/tx-begin", txUdp.begin(0) == 1);

    RtpReceiver rx(rxUdp);
    RtpSender tx(txUdp);

    EXPECT_TRUE("l16s/rx-begin", rx.begin(kPort + 1));
    rx.setFormat({48000, 2, 16}); // pre-declare so format() is correct before any packet
    EXPECT_TRUE("l16s/tx-init", tx.begin(IPAddress(127, 0, 0, 1), kPort + 1));
    EXPECT_TRUE("l16s/setFormat", tx.setFormat({48000, 2, 16}));
    EXPECT_EQ("l16s/pt", (long)pcmflowudp::RtpPayloadType::L16Stereo, (long)tx.payloadType());

    // 64 stereo frames = 128 int16 samples = 256 bytes. Below
    // samplesPerPacket so we'll need flush().
    static int16_t input[128];
    for (size_t i = 0; i < 128; ++i)
        input[i] = static_cast<int16_t>(((int)i - 64) * 100);

    EXPECT_EQ("l16s/writeFrames", 64L, (long)tx.writeFrames(input, 64));
    EXPECT_TRUE("l16s/flush", tx.flush());

    EXPECT_TRUE("l16s/rx-poll", pumpUntilProgress(rx));
    EXPECT_EQ("l16s/rx-channels", 2L, (long)rx.format().channels);
    EXPECT_EQ("l16s/rx-pt", (long)pcmflowudp::RtpPayloadType::L16Stereo, (long)rx.payloadType());

    static int16_t output[128] = {0};
    const size_t got = rx.readFrames(output, 64);
    EXPECT_EQ("l16s/readFrames", 64L, (long)got);

    int mismatches = 0;
    for (size_t i = 0; i < 128; ++i)
        if (output[i] != input[i])
            ++mismatches;
    EXPECT_EQ("l16s/sample-mismatches", 0L, (long)mismatches);

    rx.end();
    tx.end();
}

static void test_encoded_round_trip_pcmu()
{
    // Verify writeEncoded()/readEncoded() with a PCMU-shaped payload.
    // We don't run any actual G.711 here — the test only checks RTP
    // framing and byte transparency.
    WiFiUDP txUdp, rxUdp;
    EXPECT_TRUE("enc/tx-begin", txUdp.begin(0) == 1);

    RtpReceiver rx(rxUdp);
    RtpSender tx(txUdp);

    EXPECT_TRUE("enc/rx-begin", rx.begin(kPort + 2));
    EXPECT_TRUE("enc/tx-init",
                tx.begin(IPAddress(127, 0, 0, 1), kPort + 2, 0xCAFE0001));
    EXPECT_TRUE("enc/setPT",
                tx.setPayloadType(
                    static_cast<uint8_t>(pcmflowudp::RtpPayloadType::PCMU),
                    8000));
    tx.setTimestampIncrement(160); // 20 ms at 8 kHz

    uint8_t payload[160];
    for (size_t i = 0; i < 160; ++i)
        payload[i] = static_cast<uint8_t>(0xFF - (i & 0x7F));

    EXPECT_TRUE("enc/write1", tx.writeEncoded(payload, sizeof(payload)));

    EXPECT_TRUE("enc/rx-poll", pumpUntilProgress(rx));
    EXPECT_TRUE("enc/not-pcm", !rx.isPcm());
    EXPECT_EQ("enc/rx-pt",
              (long)pcmflowudp::RtpPayloadType::PCMU,
              (long)rx.payloadType());
    EXPECT_EQ("enc/rx-ssrc", (long)0xCAFE0001L, (long)rx.ssrc());
    EXPECT_TRUE("enc/rx-marker", rx.marker());
    EXPECT_TRUE("enc/has-encoded", rx.hasEncoded());

    uint8_t out[256] = {0};
    const size_t n = rx.readEncoded(out, sizeof(out));
    EXPECT_EQ("enc/readEncoded", 160L, (long)n);
    EXPECT_TRUE("enc/has-encoded-cleared", !rx.hasEncoded());
    EXPECT_TRUE("enc/payload-eq", memcmp(out, payload, sizeof(payload)) == 0);

    rx.end();
    tx.end();
}

static void test_encoded_payload_types()
{
    struct Case
    {
        uint8_t pt;
        uint32_t clock;
        uint32_t increment;
        uint16_t port;
        const char *tag;
    };

    const Case cases[] = {
        {static_cast<uint8_t>(pcmflowudp::RtpPayloadType::PCMA), 8000, 160, kPort + 20, "pcma"},
        {static_cast<uint8_t>(pcmflowudp::RtpPayloadType::G722), 8000, 160, kPort + 21, "g722"},
        {96, 48000, 960, kPort + 22, "opus96"},
    };

    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c)
    {
        WiFiUDP txUdp, rxUdp;
        EXPECT_TRUE(cases[c].tag, txUdp.begin(0) == 1);

        RtpReceiver rx(rxUdp);
        RtpSender tx(txUdp);

        EXPECT_TRUE("ptype/rx-begin", rx.begin(cases[c].port));
        EXPECT_TRUE("ptype/tx-init",
                    tx.begin(IPAddress(127, 0, 0, 1), cases[c].port, 0xABCD0000 + c));
        EXPECT_TRUE("ptype/setPT", tx.setPayloadType(cases[c].pt, cases[c].clock));
        tx.setTimestampIncrement(cases[c].increment);

        uint8_t payload[12];
        for (size_t i = 0; i < sizeof(payload); ++i)
            payload[i] = static_cast<uint8_t>(cases[c].pt + i);

        const uint32_t ts0 = tx.timestamp();
        EXPECT_TRUE("ptype/write", tx.writeEncoded(payload, sizeof(payload)));
        EXPECT_TRUE("ptype/poll", pumpUntilProgress(rx));
        EXPECT_TRUE("ptype/not-pcm", !rx.isPcm());
        EXPECT_EQ("ptype/pt", cases[c].pt, rx.payloadType());
        EXPECT_EQ("ptype/ts", (long)ts0, (long)rx.timestamp());

        uint8_t out[sizeof(payload)] = {0};
        const size_t got = rx.readEncoded(out, sizeof(out));
        EXPECT_EQ("ptype/read", (long)sizeof(payload), (long)got);
        EXPECT_TRUE("ptype/payload-eq", memcmp(out, payload, sizeof(payload)) == 0);

        rx.end();
        tx.end();
    }
}

static void test_sequence_and_marker()
{
    // Send 3 packets, verify seq increments by 1 each time and marker
    // is set only on the first.
    WiFiUDP txUdp, rxUdp;
    EXPECT_TRUE("seq/tx-begin", txUdp.begin(0) == 1);

    RtpReceiver rx(rxUdp);
    RtpSender tx(txUdp);

    EXPECT_TRUE("seq/rx-begin", rx.begin(kPort + 3));
    EXPECT_TRUE("seq/tx-init",
                tx.begin(IPAddress(127, 0, 0, 1), kPort + 3, 0xDEADBEEF));
    EXPECT_TRUE("seq/setPT",
                tx.setPayloadType(
                    static_cast<uint8_t>(pcmflowudp::RtpPayloadType::PCMU),
                    8000));
    tx.setTimestampIncrement(160);

    uint8_t payload[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    const uint16_t seq0 = tx.sequenceNumber();
    const uint32_t ts0 = tx.timestamp();

    EXPECT_TRUE("seq/p1-send", tx.writeEncoded(payload, sizeof(payload)));
    EXPECT_TRUE("seq/p2-send", tx.writeEncoded(payload, sizeof(payload)));
    EXPECT_TRUE("seq/p3-send", tx.writeEncoded(payload, sizeof(payload)));

    // Drain 3 packets, recording seq/marker on each.
    uint16_t seqs[3] = {0, 0, 0};
    bool marks[3] = {false, false, false};
    uint32_t timestamps[3] = {0, 0, 0};
    for (int i = 0; i < 3; ++i)
    {
        bool got = false;
        for (int j = 0; j < 100 && !got; ++j)
        {
            if (rx.poll())
                got = true;
            else
                delay(2);
        }
        EXPECT_TRUE("seq/got-packet", got);
        seqs[i] = rx.sequenceNumber();
        marks[i] = rx.marker();
        timestamps[i] = rx.timestamp();
        // Consume the encoded hold so the next poll() writes fresh data.
        uint8_t scratch[16];
        (void)rx.readEncoded(scratch, sizeof(scratch));
    }

    EXPECT_EQ("seq/seq0", (long)seq0,                (long)seqs[0]);
    EXPECT_EQ("seq/seq1", (long)((seq0 + 1) & 0xFFFF), (long)seqs[1]);
    EXPECT_EQ("seq/seq2", (long)((seq0 + 2) & 0xFFFF), (long)seqs[2]);
    EXPECT_TRUE("seq/marker0",  marks[0]);
    EXPECT_TRUE("seq/marker1", !marks[1]);
    EXPECT_TRUE("seq/marker2", !marks[2]);
    EXPECT_EQ("seq/ts0", (long)ts0,         (long)timestamps[0]);
    EXPECT_EQ("seq/ts1", (long)(ts0 + 160), (long)timestamps[1]);
    EXPECT_EQ("seq/ts2", (long)(ts0 + 320), (long)timestamps[2]);

    rx.end();
    tx.end();
}

void setup()
{
    Serial.begin(115200);
    delay(5000); // ESP32 Serial-USB bridge can drop early output otherwise

    test_l16_mono_round_trip();
    test_l16_stereo_round_trip();
    test_encoded_round_trip_pcmu();
    test_encoded_payload_types();
    test_sequence_and_marker();

    Serial.print("TEST done ");
    Serial.print(g_pass);
    Serial.print("/");
    Serial.println(g_total);
}

void loop()
{
    delay(1);
}
