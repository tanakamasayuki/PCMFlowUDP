// Unit tests for RTP fixed-header pack / parse (RFC 3550 §5.1).
//
// Verifies the on-the-wire byte layout against an independently
// computed reference for known configurations. RTP's header is fully
// deterministic, so the check is bit-exact.
//
// Reference test case (PCMU, byte 0 = 0x80 = V=2,P=0,X=0,CC=0):
//   V=2, P=0, X=0, CC=0, M=0, PT=0 (PCMU)
//   seq    = 0x1234
//   ts     = 0xCAFEBABE
//   ssrc   = 0xDEADBEEF
// Expected: 80 00 12 34 CA FE BA BE DE AD BE EF

#include <PCMFlowUDP.h>
#include <string.h>

using pcmflowudp::encodeRtpHeader;
using pcmflowudp::kRtpHeaderBytes;
using pcmflowudp::parseRtpHeader;
using pcmflowudp::RtpHeader;
using pcmflowudp::RtpParseResult;
using pcmflowudp::RtpPayloadType;

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

static const uint8_t kReference[kRtpHeaderBytes] = {
    0x80, // V=2, P=0, X=0, CC=0
    0x00, // M=0, PT=0 (PCMU)
    0x12,
    0x34, // seq = 0x1234 (BE)
    0xCA,
    0xFE,
    0xBA,
    0xBE, // ts  = 0xCAFEBABE (BE)
    0xDE,
    0xAD,
    0xBE,
    0xEF, // ssrc = 0xDEADBEEF (BE)
};

static RtpHeader referenceHeader()
{
    RtpHeader h{};
    h.payloadType = static_cast<uint8_t>(RtpPayloadType::PCMU);
    h.sequenceNumber = 0x1234;
    h.timestamp = 0xCAFEBABE;
    h.ssrc = 0xDEADBEEF;
    return h;
}

static void test_encode_byte_exact()
{
    uint8_t out[kRtpHeaderBytes] = {0};
    EXPECT_TRUE("encode/ok", encodeRtpHeader(referenceHeader(), out));
    for (size_t i = 0; i < kRtpHeaderBytes; ++i)
    {
        char name[24];
        snprintf(name, sizeof(name), "encode/byte%02u", (unsigned)i);
        EXPECT_EQ(name, (long)kReference[i], (long)out[i]);
    }
}

static void test_parse_byte_exact()
{
    RtpHeader h{};
    EXPECT_EQ("parse/ok",
              (long)RtpParseResult::Ok,
              (long)parseRtpHeader(kReference, kRtpHeaderBytes, h));
    EXPECT_EQ("parse/version", 2L, (long)h.version);
    EXPECT_EQ("parse/padding", 0L, (long)h.padding);
    EXPECT_EQ("parse/extension", 0L, (long)h.extension);
    EXPECT_EQ("parse/csrcCount", 0L, (long)h.csrcCount);
    EXPECT_EQ("parse/marker", 0L, (long)h.marker);
    EXPECT_EQ("parse/pt", (long)RtpPayloadType::PCMU, (long)h.payloadType);
    EXPECT_EQ("parse/seq", 0x1234L, (long)h.sequenceNumber);
    EXPECT_EQ("parse/ts", 0xCAFEBABEL, (long)h.timestamp);
    EXPECT_EQ("parse/ssrc", (long)0xDEADBEEFL, (long)h.ssrc);
    EXPECT_EQ("parse/payloadOff", (long)kRtpHeaderBytes, (long)h.payloadOffset);
}

static void test_marker_and_dynamic_pt()
{
    // Opus-style dynamic PT 96 with marker set.
    RtpHeader src{};
    src.marker = true;
    src.payloadType = 96;
    src.sequenceNumber = 0xAA55;
    src.timestamp = 0x01020304;
    src.ssrc = 0x11223344;

    uint8_t buf[kRtpHeaderBytes] = {0};
    EXPECT_TRUE("mark/encode", encodeRtpHeader(src, buf));
    EXPECT_EQ("mark/byte0", 0x80L, (long)buf[0]);
    EXPECT_EQ("mark/byte1", 0xE0L, (long)buf[1]); // M=1 | PT=96

    RtpHeader dst{};
    EXPECT_EQ("mark/parse",
              (long)RtpParseResult::Ok,
              (long)parseRtpHeader(buf, kRtpHeaderBytes, dst));
    EXPECT_EQ("mark/marker", 1L, (long)dst.marker);
    EXPECT_EQ("mark/pt", 96L, (long)dst.payloadType);
    EXPECT_EQ("mark/seq", 0xAA55L, (long)dst.sequenceNumber);
}

static void test_round_trip_all_static_pts()
{
    const RtpPayloadType pts[] = {
        RtpPayloadType::PCMU,
        RtpPayloadType::PCMA,
        RtpPayloadType::G722,
        RtpPayloadType::L16Stereo,
        RtpPayloadType::L16Mono,
    };
    for (size_t i = 0; i < sizeof(pts) / sizeof(pts[0]); ++i)
    {
        RtpHeader src = referenceHeader();
        src.payloadType = static_cast<uint8_t>(pts[i]);
        src.sequenceNumber = static_cast<uint16_t>(i + 1);
        src.timestamp = static_cast<uint32_t>((i + 1) * 160u);

        uint8_t buf[kRtpHeaderBytes] = {0};
        char tag[16];
        snprintf(tag, sizeof(tag), "rt/pt%u/encode", (unsigned)src.payloadType);
        EXPECT_TRUE(tag, encodeRtpHeader(src, buf));

        RtpHeader dst{};
        snprintf(tag, sizeof(tag), "rt/pt%u/parse", (unsigned)src.payloadType);
        EXPECT_EQ(tag, (long)RtpParseResult::Ok,
                  (long)parseRtpHeader(buf, kRtpHeaderBytes, dst));
        snprintf(tag, sizeof(tag), "rt/pt%u/eq", (unsigned)src.payloadType);
        EXPECT_TRUE(tag,
                    dst.payloadType == src.payloadType &&
                        dst.sequenceNumber == src.sequenceNumber &&
                        dst.timestamp == src.timestamp &&
                        dst.ssrc == src.ssrc);
    }
}

static void test_parse_with_csrcs()
{
    // Synthesize a CC=2 packet: 12 + 4*2 = 20 byte header total, then
    // a single byte of "payload".
    uint8_t buf[20 + 1] = {0};
    memcpy(buf, kReference, kRtpHeaderBytes);
    buf[0] = 0x82; // V=2, CC=2
    // CSRC IDs (don't care about contents)
    buf[12] = 0xAA;
    buf[13] = 0xBB;
    buf[14] = 0xCC;
    buf[15] = 0xDD;
    buf[16] = 0x11;
    buf[17] = 0x22;
    buf[18] = 0x33;
    buf[19] = 0x44;
    buf[20] = 0xEE; // payload byte

    RtpHeader h{};
    EXPECT_EQ("csrc/parse",
              (long)RtpParseResult::Ok,
              (long)parseRtpHeader(buf, sizeof(buf), h));
    EXPECT_EQ("csrc/count", 2L, (long)h.csrcCount);
    EXPECT_EQ("csrc/payloadOff", 20L, (long)h.payloadOffset);
    EXPECT_EQ("csrc/payload-byte", 0xEEL, (long)buf[h.payloadOffset]);
}

static void test_parse_with_extension()
{
    // Synthesize an X=1 packet with a 2-word extension after the 12-byte
    // fixed header: 4 + 2*4 = 12 ext bytes; payload starts at 24.
    uint8_t buf[24 + 2] = {0};
    memcpy(buf, kReference, kRtpHeaderBytes);
    buf[0] = 0x90; // V=2, X=1, CC=0
    // Extension defined-by-profile (16 bits) + length=2 (16 bits BE).
    buf[12] = 0xBE;
    buf[13] = 0xDE;
    buf[14] = 0x00;
    buf[15] = 0x02;
    // 8 bytes of extension content
    for (size_t i = 16; i < 24; ++i)
        buf[i] = static_cast<uint8_t>(i);
    // 2 bytes of payload
    buf[24] = 0x77;
    buf[25] = 0x88;

    RtpHeader h{};
    EXPECT_EQ("ext/parse",
              (long)RtpParseResult::Ok,
              (long)parseRtpHeader(buf, sizeof(buf), h));
    EXPECT_EQ("ext/x-bit", 1L, (long)h.extension);
    EXPECT_EQ("ext/payloadOff", 24L, (long)h.payloadOffset);
    EXPECT_EQ("ext/payload-byte0", 0x77L, (long)buf[h.payloadOffset]);
}

static void test_parse_errors()
{
    RtpHeader h{};

    // Too short.
    EXPECT_EQ("err/too-short",
              (long)RtpParseResult::TooShort,
              (long)parseRtpHeader(kReference, 8, h));

    // Bad version (encode V=1 in bits 7..6).
    uint8_t bad[kRtpHeaderBytes];
    memcpy(bad, kReference, kRtpHeaderBytes);
    bad[0] = 0x40; // V=1
    EXPECT_EQ("err/bad-version",
              (long)RtpParseResult::BadVersion,
              (long)parseRtpHeader(bad, kRtpHeaderBytes, h));

    // CSRC count claims more bytes than buffer holds.
    memcpy(bad, kReference, kRtpHeaderBytes);
    bad[0] = 0x8F; // CC=15 -> needs 12 + 60 = 72 bytes
    EXPECT_EQ("err/csrc-truncated",
              (long)RtpParseResult::TooShort,
              (long)parseRtpHeader(bad, kRtpHeaderBytes, h));
}

static void test_encode_errors()
{
    RtpHeader h = referenceHeader();
    uint8_t buf[kRtpHeaderBytes] = {0};

    h.payloadType = 128;
    EXPECT_TRUE("err/pt-128", !encodeRtpHeader(h, buf));

    h = referenceHeader();
    EXPECT_TRUE("err/null-out", !encodeRtpHeader(h, nullptr));
}

void setup()
{
    Serial.begin(115200);
    delay(5000); // ESP32 Serial-USB bridge can drop early output otherwise

    test_encode_byte_exact();
    test_parse_byte_exact();
    test_marker_and_dynamic_pt();
    test_round_trip_all_static_pts();
    test_parse_with_csrcs();
    test_parse_with_extension();
    test_parse_errors();
    test_encode_errors();

    Serial.print("TEST done ");
    Serial.print(g_pass);
    Serial.print("/");
    Serial.println(g_total);
}

void loop()
{
    delay(1);
}
