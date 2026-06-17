// End-to-end test for VbanSender + VbanReceiver over loopback UDP.
//
// PCM16 samples are sent through a VBAN audio packet and read back on
// the same host. Round-trip is bit-exact (PCM16 is a no-op codec).
//
// HOST PROFILE ONLY. The arduino-esp32 build of lwIP does not enable
// the loopback interface (LWIP_HAVE_LOOPIF), so the same-process
// 127.0.0.1 pattern used here cannot run on ESP32 hardware. The
// on-wire VBAN path is exercised on real hardware by
// vban_python_loopback/. See tests/README.md "ESP32 vs host".

#include <WiFiUdp.h>
#include <PCMFlowUDP.h>

static int g_pass = 0;
static int g_total = 0;

#define EXPECT_TRUE(name, cond)      \
    do                               \
    {                                \
        ++g_total;                   \
        if (cond)                    \
        {                            \
            ++g_pass;                \
            Serial.print("PASS ");   \
            Serial.println(name);    \
        }                            \
        else                         \
        {                            \
            Serial.print("FAIL ");   \
            Serial.print(name);      \
            Serial.println(" cond"); \
        }                            \
    } while (0)

#define EXPECT_EQ(name, expected, actual) \
    do                                    \
    {                                     \
        ++g_total;                        \
        const long _e = (long)(expected); \
        const long _a = (long)(actual);   \
        if (_e == _a)                     \
        {                                 \
            ++g_pass;                     \
            Serial.print("PASS ");        \
            Serial.println(name);         \
        }                                 \
        else                              \
        {                                 \
            Serial.print("FAIL ");        \
            Serial.print(name);           \
            Serial.print(" expected=");   \
            Serial.print(_e);             \
            Serial.print(" actual=");     \
            Serial.println(_a);           \
        }                                 \
    } while (0)

static constexpr uint16_t kPort = 47200;

// Pump the receiver for up to ~500 ms.
static bool pumpUntilProgress(VbanReceiver &r)
{
    for (int i = 0; i < 100; ++i)
    {
        if (r.poll())
            return true;
        delay(5);
    }
    return false;
}

static void test_mono_round_trip()
{
    WiFiUDP txUdp, rxUdp;
    EXPECT_TRUE("mono/tx-begin", txUdp.begin(0) == 1);

    VbanReceiver rx(rxUdp);
    VbanSender tx(txUdp);

    EXPECT_TRUE("mono/rx-begin", rx.begin(kPort, "MonoTest"));
    EXPECT_TRUE("mono/tx-init", tx.begin(IPAddress(127, 0, 0, 1), kPort, "MonoTest"));
    EXPECT_TRUE("mono/setFormat", tx.setFormat({16000, 1, 16}));

    static int16_t input[256];
    for (size_t i = 0; i < 256; ++i)
        input[i] = static_cast<int16_t>(i * 100 - 12800);

    EXPECT_EQ("mono/writeFrames", 256L,
              (long)tx.writeFrames(input, 256));

    EXPECT_TRUE("mono/rx-poll", pumpUntilProgress(rx));
    EXPECT_EQ("mono/rate", 16000L, (long)rx.format().sampleRate);
    EXPECT_EQ("mono/channels", 1L, (long)rx.format().channels);
    EXPECT_EQ("mono/bits", 16L, (long)rx.format().bitsPerSample);

    static int16_t output[256] = {0};
    const size_t got = rx.readFrames(output, 256);
    EXPECT_EQ("mono/readFrames", 256L, (long)got);

    int mismatches = 0;
    for (size_t i = 0; i < 256; ++i)
        if (output[i] != input[i])
            ++mismatches;
    EXPECT_EQ("mono/sample-mismatches", 0L, (long)mismatches);

    rx.end();
    tx.end();
}

static void test_stereo_round_trip()
{
    WiFiUDP txUdp, rxUdp;
    EXPECT_TRUE("stereo/tx-begin", txUdp.begin(0) == 1);

    VbanReceiver rx(rxUdp);
    VbanSender tx(txUdp);

    EXPECT_TRUE("stereo/rx-begin", rx.begin(kPort + 1));
    EXPECT_TRUE("stereo/tx-init", tx.begin(IPAddress(127, 0, 0, 1), kPort + 1, "StStream"));
    EXPECT_TRUE("stereo/setFormat", tx.setFormat({48000, 2, 16}));

    // 64 stereo frames = 128 int16 samples (L,R interleaved).
    static int16_t input[128];
    for (size_t i = 0; i < 128; ++i)
        input[i] = static_cast<int16_t>(((int)i - 64) * 200);

    EXPECT_EQ("stereo/writeFrames", 64L,
              (long)tx.writeFrames(input, 64));
    // 64 frames < samplesPerPacket(256) — flush to force the partial
    // packet onto the wire.
    EXPECT_TRUE("stereo/flush", tx.flush());

    EXPECT_TRUE("stereo/rx-poll", pumpUntilProgress(rx));
    EXPECT_EQ("stereo/channels", 2L, (long)rx.format().channels);
    EXPECT_EQ("stereo/rate", 48000L, (long)rx.format().sampleRate);

    static int16_t output[128] = {0};
    const size_t got = rx.readFrames(output, 64);
    EXPECT_EQ("stereo/readFrames", 64L, (long)got);

    int mismatches = 0;
    for (size_t i = 0; i < 128; ++i)
        if (output[i] != input[i])
            ++mismatches;
    EXPECT_EQ("stereo/sample-mismatches", 0L, (long)mismatches);

    rx.end();
    tx.end();
}

static void test_multi_packet()
{
    // 600 mono samples at 256 samples/packet -> 3 packets (256+256+88).
    WiFiUDP txUdp, rxUdp;
    EXPECT_TRUE("multi/tx-begin", txUdp.begin(0) == 1);

    VbanReceiver rx(rxUdp);
    VbanSender tx(txUdp);

    EXPECT_TRUE("multi/rx-begin", rx.begin(kPort + 2));
    EXPECT_TRUE("multi/tx-init", tx.begin(IPAddress(127, 0, 0, 1), kPort + 2, "Multi"));
    EXPECT_TRUE("multi/setFormat", tx.setFormat({16000, 1, 16}));

    static int16_t input[600];
    for (size_t i = 0; i < 600; ++i)
        input[i] = static_cast<int16_t>(i);

    // writeFrames sends the two full packets; the trailing 88 frames
    // stay buffered. We need a small write that doesn't fill a packet
    // to confirm partial-buffer behavior; for now exercise just the
    // full-packet path.
    EXPECT_EQ("multi/writeFrames", 600L, (long)tx.writeFrames(input, 600));
    // The two full packets (2*256=512 frames) were sent inline by
    // writeFrames; flush() forces the trailing 88-frame partial packet.
    EXPECT_TRUE("multi/flush", tx.flush());

    static int16_t output[600] = {0};
    size_t totalGot = 0;
    for (int i = 0; i < 50 && totalGot < 600; ++i)
    {
        rx.poll();
        totalGot += rx.readFrames(output + totalGot, 600 - totalGot);
        delay(5);
    }
    EXPECT_EQ("multi/readFrames", 600L, (long)totalGot);

    int mismatches = 0;
    for (size_t i = 0; i < 600; ++i)
        if (output[i] != input[i])
            ++mismatches;
    EXPECT_EQ("multi/sample-mismatches", 0L, (long)mismatches);

    rx.end();
    tx.end();
}

static void test_stream_name_filter()
{
    WiFiUDP txUdp, rxUdp;
    EXPECT_TRUE("filter/tx-begin", txUdp.begin(0) == 1);

    VbanReceiver rx(rxUdp);
    VbanSender tx(txUdp);

    EXPECT_TRUE("filter/rx-begin", rx.begin(kPort + 3, "Wanted"));
    EXPECT_TRUE("filter/tx-init", tx.begin(IPAddress(127, 0, 0, 1), kPort + 3, "Other"));
    EXPECT_TRUE("filter/setFormat", tx.setFormat({16000, 1, 16}));

    static int16_t input[256] = {0};
    for (size_t i = 0; i < 256; ++i)
        input[i] = static_cast<int16_t>(i);
    EXPECT_EQ("filter/writeFrames", 256L, (long)tx.writeFrames(input, 256));
    // 256 frames exactly fills one packet so writeFrames emits inline,
    // but call flush() defensively in case the default changes later.
    EXPECT_TRUE("filter/flush", tx.flush());

    // Pump for a while; the packet arrives but the name doesn't match
    // so poll() returns false and the queue stays empty.
    bool sawAccept = false;
    for (int i = 0; i < 50; ++i)
    {
        if (rx.poll())
        {
            sawAccept = true;
            break;
        }
        delay(5);
    }
    EXPECT_TRUE("filter/no-accept", !sawAccept);

    int16_t dummy[8] = {0};
    EXPECT_EQ("filter/no-data", 0L, (long)rx.readFrames(dummy, 8));

    rx.end();
    tx.end();
}

// Captured by the service callback. Static to survive past callback return.
static struct
{
    bool fired = false;
    uint8_t function = 0xFF;
    uint8_t flags = 0xFF;
    uint32_t frameCounter = 0;
    char streamName[17] = {0};
    uint16_t fromPort = 0;
    uint8_t payload[64] = {0};
    size_t payloadBytes = 0;
} g_svc;

static void onService(const VbanServicePacket &pkt, void *)
{
    g_svc.fired = true;
    g_svc.function = pkt.header.serviceFunction;
    g_svc.flags = pkt.header.serviceFlags;
    g_svc.frameCounter = pkt.header.frameCounter;
    memcpy(g_svc.streamName, pkt.header.streamName, 16);
    g_svc.fromPort = pkt.fromPort;
    const size_t n = (pkt.payloadBytes < sizeof(g_svc.payload))
                         ? pkt.payloadBytes
                         : sizeof(g_svc.payload);
    memcpy(g_svc.payload, pkt.payload, n);
    g_svc.payloadBytes = n;
}

static void test_service_callback()
{
    WiFiUDP txUdp, rxUdp;
    EXPECT_TRUE("svc/tx-begin", txUdp.begin(0) == 1);

    VbanReceiver rx(rxUdp);
    EXPECT_TRUE("svc/rx-begin", rx.begin(kPort + 4));
    rx.setServiceCallback(&onService, nullptr);

    // Build and send a Service identification request directly via UDP
    // (Service packets bypass VbanSender — they aren't audio).
    pcmflowudp::VbanServiceHeader sh{};
    sh.serviceFunction = pcmflowudp::kVbanServiceIdentification;
    sh.serviceFlags = 0; // request
    strncpy(sh.streamName, "PingTest", pcmflowudp::kVbanStreamNameBytes);
    sh.frameCounter = 0x01020304;

    uint8_t pkt[pcmflowudp::kVbanHeaderBytes + 5];
    EXPECT_TRUE("svc/encode", pcmflowudp::encodeServiceHeader(sh, pkt));
    // Synthetic 5-byte payload to verify the callback gets payload too.
    const uint8_t payload[] = {'h', 'e', 'l', 'l', 'o'};
    memcpy(pkt + pcmflowudp::kVbanHeaderBytes, payload, sizeof(payload));

    EXPECT_TRUE("svc/tx-bp",
                txUdp.beginPacket(IPAddress(127, 0, 0, 1), kPort + 4) == 1);
    EXPECT_EQ("svc/tx-write", (long)sizeof(pkt),
              (long)txUdp.write(pkt, sizeof(pkt)));
    EXPECT_TRUE("svc/tx-ep", txUdp.endPacket() == 1);

    // Pump until the callback fires.
    bool processed = false;
    for (int i = 0; i < 100; ++i)
    {
        if (rx.poll())
        {
            processed = true;
            break;
        }
        delay(5);
    }
    EXPECT_TRUE("svc/poll-processed", processed);
    EXPECT_TRUE("svc/cb-fired", g_svc.fired);
    EXPECT_EQ("svc/cb-func", 0L, (long)g_svc.function);
    EXPECT_EQ("svc/cb-flags", 0L, (long)g_svc.flags);
    EXPECT_EQ("svc/cb-frameCnt", 0x01020304L, (long)g_svc.frameCounter);
    EXPECT_TRUE("svc/cb-name", strncmp(g_svc.streamName, "PingTest", 8) == 0);
    EXPECT_EQ("svc/cb-payload-len", 5L, (long)g_svc.payloadBytes);
    EXPECT_TRUE("svc/cb-payload-eq",
                memcmp(g_svc.payload, payload, sizeof(payload)) == 0);

    rx.end();
}

void setup()
{
    Serial.begin(115200);
    delay(5000); // ESP32 Serial-USB bridge can drop early output otherwise

    test_mono_round_trip();
    test_stereo_round_trip();
    test_multi_packet();
    test_stream_name_filter();
    test_service_callback();

    Serial.print("TEST done ");
    Serial.print(g_pass);
    Serial.print("/");
    Serial.println(g_total);
}

void loop()
{
    delay(1);
}
