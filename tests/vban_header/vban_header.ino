// Unit tests for VBAN audio-header pack / parse.
//
// Verifies the on-the-wire byte layout against an independently
// computed reference for a known configuration. VBAN's audio header
// is fully deterministic, so the check is bit-exact.
//
// Reference test case (chosen so every header byte is non-trivial):
//   subProtocol = Audio (0x00)
//   sampleRate  = 16000 Hz   -> index 8 in the VBAN sample-rate table
//   numSamples  = 128        -> wire byte 5 = 0x7F  (N-1)
//   numChannels = 1          -> wire byte 6 = 0x00
//   subCodec    = PCM16      -> wire byte 7 = 0x01
//   streamName  = "TestStream" + 6 zero-pad bytes
//   frameCnt    = 0x12345678 -> little-endian 78 56 34 12

#include <PCMFlowUDP.h>
#include <string.h>

using pcmflowudp::encodeAudioHeader;
using pcmflowudp::encodeServiceHeader;
using pcmflowudp::kVbanHeaderBytes;
using pcmflowudp::kVbanServiceIdentification;
using pcmflowudp::kVbanServiceReplyFlag;
using pcmflowudp::parseAudioHeader;
using pcmflowudp::parseServiceHeader;
using pcmflowudp::VbanAudioHeader;
using pcmflowudp::VbanParseResult;
using pcmflowudp::VbanServiceHeader;
using pcmflowudp::VbanSubCodec;

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

static const uint8_t kReference[kVbanHeaderBytes] = {
    'V',
    'B',
    'A',
    'N',  // [0..3]
    0x08, // [4]  Audio | srIdx=8
    0x7F, // [5]  128-1
    0x00, // [6]  1-1
    0x01, // [7]  PCM16
    'T',
    'e',
    's',
    't',
    'S',
    't',
    'r',
    'e',
    'a',
    'm', // [8..17]
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00, // [18..23]
    0x78,
    0x56,
    0x34,
    0x12, // [24..27] LE 0x12345678
};

static VbanAudioHeader referenceHeader()
{
    VbanAudioHeader h{};
    h.sampleRateIndex = 8;
    h.numSamples = 128;
    h.numChannels = 1;
    h.subCodec = VbanSubCodec::PCM16;
    strncpy(h.streamName, "TestStream", pcmflowudp::kVbanStreamNameBytes);
    h.frameCounter = 0x12345678;
    return h;
}

static void test_encode_byte_exact()
{
    uint8_t out[kVbanHeaderBytes] = {0};
    EXPECT_TRUE("encode/ok", encodeAudioHeader(referenceHeader(), out));
    for (size_t i = 0; i < kVbanHeaderBytes; ++i)
    {
        char name[24];
        snprintf(name, sizeof(name), "encode/byte%02u", (unsigned)i);
        EXPECT_EQ(name, (long)kReference[i], (long)out[i]);
    }
}

static void test_parse_byte_exact()
{
    VbanAudioHeader h{};
    EXPECT_EQ("parse/ok",
              (long)VbanParseResult::Ok,
              (long)parseAudioHeader(kReference, kVbanHeaderBytes, h));
    EXPECT_EQ("parse/srIdx", 8L, (long)h.sampleRateIndex);
    EXPECT_EQ("parse/samples", 128L, (long)h.numSamples);
    EXPECT_EQ("parse/channels", 1L, (long)h.numChannels);
    EXPECT_EQ("parse/codec", (long)VbanSubCodec::PCM16, (long)h.subCodec);
    EXPECT_EQ("parse/frameCnt", 0x12345678L, (long)h.frameCounter);
    EXPECT_TRUE("parse/name", strcmp(h.streamName, "TestStream") == 0);
}

static void test_round_trip()
{
    uint8_t buf[kVbanHeaderBytes] = {0};
    VbanAudioHeader src = referenceHeader();
    src.frameCounter = 0xDEADBEEF;
    src.numSamples = 256;  // wire boundary
    src.numChannels = 256; // wire boundary
    src.subCodec = VbanSubCodec::MuLaw;
    src.sampleRateIndex = 20; // max valid

    EXPECT_TRUE("rt/encode", encodeAudioHeader(src, buf));

    // Boundary wire bytes
    EXPECT_EQ("rt/wire-samples", 0xFFL, (long)buf[5]);
    EXPECT_EQ("rt/wire-channels", 0xFFL, (long)buf[6]);
    EXPECT_EQ("rt/wire-codec", (long)VbanSubCodec::MuLaw, (long)buf[7]);
    EXPECT_EQ("rt/wire-sr", 0x00L | 20L, (long)buf[4]);

    VbanAudioHeader dst{};
    EXPECT_EQ("rt/parse",
              (long)VbanParseResult::Ok,
              (long)parseAudioHeader(buf, kVbanHeaderBytes, dst));
    EXPECT_EQ("rt/srIdx", (long)src.sampleRateIndex, (long)dst.sampleRateIndex);
    EXPECT_EQ("rt/samples", (long)src.numSamples, (long)dst.numSamples);
    EXPECT_EQ("rt/channels", (long)src.numChannels, (long)dst.numChannels);
    EXPECT_EQ("rt/codec", (long)src.subCodec, (long)dst.subCodec);
    EXPECT_EQ("rt/frameCnt", (long)src.frameCounter, (long)dst.frameCounter);
}

static void test_parse_errors()
{
    VbanAudioHeader h{};

    // Too short.
    EXPECT_EQ("err/too-short",
              (long)VbanParseResult::TooShort,
              (long)parseAudioHeader(kReference, 10, h));

    // Bad signature.
    uint8_t bad[kVbanHeaderBytes];
    memcpy(bad, kReference, kVbanHeaderBytes);
    bad[0] = 'X';
    EXPECT_EQ("err/bad-sig",
              (long)VbanParseResult::BadSignature,
              (long)parseAudioHeader(bad, kVbanHeaderBytes, h));

    // Service sub-protocol -> NotAudio.
    memcpy(bad, kReference, kVbanHeaderBytes);
    bad[4] = 0x60 | 0x08; // Service | srIdx
    EXPECT_EQ("err/not-audio",
              (long)VbanParseResult::NotAudio,
              (long)parseAudioHeader(bad, kVbanHeaderBytes, h));
}

static void test_encode_errors()
{
    uint8_t buf[kVbanHeaderBytes] = {0};
    VbanAudioHeader h = referenceHeader();

    h.numSamples = 0;
    EXPECT_TRUE("err/samples-0", !encodeAudioHeader(h, buf));
    h.numSamples = 257;
    EXPECT_TRUE("err/samples-257", !encodeAudioHeader(h, buf));

    h = referenceHeader();
    h.numChannels = 0;
    EXPECT_TRUE("err/channels-0", !encodeAudioHeader(h, buf));

    h = referenceHeader();
    h.sampleRateIndex = 21;
    EXPECT_TRUE("err/sr-21", !encodeAudioHeader(h, buf));

    h = referenceHeader();
    EXPECT_TRUE("err/null-out", !encodeAudioHeader(h, nullptr));
}

// VBAN Service-sub-protocol reference packet (identification request).
//   serviceFunction = 0 (Identification)
//   serviceFlags    = 0 (request, no reply bit)
//   streamName      = "VBAN-Service"
//   frameCounter    = 0xCAFEBABE
static const uint8_t kServiceReference[kVbanHeaderBytes] = {
    'V',
    'B',
    'A',
    'N',  // [0..3]
    0x60, // [4]  Service | func=0
    0x00, // [5]  flags = request
    0x00, // [6]
    0x00, // [7]
    'V',
    'B',
    'A',
    'N',
    '-',
    'S',
    'e',
    'r',
    'v',
    'i',
    'c',
    'e', // [8..19]
    0x00,
    0x00,
    0x00,
    0x00, // [20..23]
    0xBE,
    0xBA,
    0xFE,
    0xCA, // [24..27] LE 0xCAFEBABE
};

static void test_service_encode_byte_exact()
{
    VbanServiceHeader h{};
    h.serviceFunction = kVbanServiceIdentification; // 0x00
    h.serviceFlags = 0;
    strncpy(h.streamName, "VBAN-Service", pcmflowudp::kVbanStreamNameBytes);
    h.frameCounter = 0xCAFEBABE;

    uint8_t out[kVbanHeaderBytes] = {0};
    EXPECT_TRUE("svc/encode-ok", encodeServiceHeader(h, out));
    for (size_t i = 0; i < kVbanHeaderBytes; ++i)
    {
        char name[24];
        snprintf(name, sizeof(name), "svc/byte%02u", (unsigned)i);
        EXPECT_EQ(name, (long)kServiceReference[i], (long)out[i]);
    }
}

static void test_service_parse()
{
    VbanServiceHeader h{};
    EXPECT_EQ("svc/parse-ok",
              (long)VbanParseResult::Ok,
              (long)parseServiceHeader(kServiceReference, kVbanHeaderBytes, h));
    EXPECT_EQ("svc/parse-func", 0L, (long)h.serviceFunction);
    EXPECT_EQ("svc/parse-flags", 0L, (long)h.serviceFlags);
    EXPECT_TRUE("svc/parse-is-request", !h.isReply());
    EXPECT_EQ("svc/parse-frameCnt", 0xCAFEBABEL, (long)h.frameCounter);
    EXPECT_TRUE("svc/parse-name", strcmp(h.streamName, "VBAN-Service") == 0);

    // Audio packet must not be misparsed as Service.
    EXPECT_EQ("svc/not-service",
              (long)VbanParseResult::NotService,
              (long)parseServiceHeader(kReference, kVbanHeaderBytes, h));
}

static void test_service_reply_flag()
{
    VbanServiceHeader h{};
    h.serviceFunction = kVbanServiceIdentification;
    h.serviceFlags = kVbanServiceReplyFlag; // 0x80
    strncpy(h.streamName, "Reply", pcmflowudp::kVbanStreamNameBytes);

    uint8_t buf[kVbanHeaderBytes] = {0};
    EXPECT_TRUE("svc/reply-encode", encodeServiceHeader(h, buf));
    EXPECT_EQ("svc/reply-byte4", 0x60L, (long)buf[4]);
    EXPECT_EQ("svc/reply-byte5", 0x80L, (long)buf[5]);

    VbanServiceHeader dst{};
    EXPECT_EQ("svc/reply-parse",
              (long)VbanParseResult::Ok,
              (long)parseServiceHeader(buf, kVbanHeaderBytes, dst));
    EXPECT_TRUE("svc/reply-is-reply", dst.isReply());
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    test_encode_byte_exact();
    test_parse_byte_exact();
    test_round_trip();
    test_parse_errors();
    test_encode_errors();
    test_service_encode_byte_exact();
    test_service_parse();
    test_service_reply_flag();

    Serial.print("TEST done ");
    Serial.print(g_pass);
    Serial.print("/");
    Serial.println(g_total);
}

void loop()
{
    delay(1);
}
