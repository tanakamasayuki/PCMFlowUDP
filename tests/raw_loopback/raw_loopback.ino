// Unit / integration test for RawUdpSink + RawUdpStream.
//
// Two WiFiUDP instances on the loopback interface exchange a small
// known payload; the receiver-side bytes are asserted byte-for-byte
// against the sender's input.
//
// On lang-ship:host this exercises real BSD sockets via the host
// Arduino core's WiFiUDP. On esp32 the same sketch runs against the
// real WiFi stack (manual setup; not part of the automated CI run).

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

static constexpr uint16_t kPort = 47100;
static const uint8_t kPayload[] = {0x01, 0x02, 0xFF, 0x55, 0xAA, 0x00, 0x7F, 0x80};

WiFiUDP g_txUdp;
WiFiUDP g_rxUdp;
RawUdpSink g_sink(g_txUdp);
RawUdpStream g_stream(g_rxUdp);

static void test_loopback()
{
    // The TX-side UDP needs an ephemeral local binding so sendto() has
    // a source socket. Matches Arduino's documented usage of WiFiUDP
    // (begin() before any beginPacket/endPacket).
    EXPECT_TRUE("tx/begin", g_txUdp.begin(0) == 1);
    EXPECT_TRUE("stream/begin", g_stream.begin(kPort));
    EXPECT_TRUE("sink/begin", g_sink.begin(IPAddress(127, 0, 0, 1), kPort));

    EXPECT_EQ("write/count", (long)sizeof(kPayload),
              (long)g_sink.write(kPayload, sizeof(kPayload)));
    EXPECT_TRUE("flush", g_sink.flush());

    // Poll the receiver. The send completed synchronously on the host;
    // give a short loopback budget for any kernel scheduling.
    bool got = false;
    for (int i = 0; i < 100; ++i)
    {
        if (g_stream.poll())
        {
            got = true;
            break;
        }
        delay(5);
    }
    EXPECT_TRUE("poll/recv", got);

    uint8_t buf[32] = {0};
    const size_t n = g_stream.read(buf, sizeof(buf));
    EXPECT_EQ("read/count", (long)sizeof(kPayload), (long)n);
    for (size_t i = 0; i < sizeof(kPayload); ++i)
    {
        char name[16];
        snprintf(name, sizeof(name), "read/b%u", (unsigned)i);
        EXPECT_EQ(name, (long)kPayload[i], (long)buf[i]);
    }

    EXPECT_EQ("remote/port", (long)g_txUdp.localPort(),
              (long)g_stream.remotePort());
}

static void test_empty_flush()
{
    // Flushing an empty sink is a no-op success, not an error.
    EXPECT_TRUE("empty-flush", g_sink.flush());
}

static void test_chunked_write()
{
    // Multiple writes accumulate into one datagram until flush().
    const uint8_t a[] = {0xDE, 0xAD};
    const uint8_t b[] = {0xBE, 0xEF};
    EXPECT_EQ("write/a", 2L, (long)g_sink.write(a, sizeof(a)));
    EXPECT_EQ("write/b", 2L, (long)g_sink.write(b, sizeof(b)));
    EXPECT_TRUE("flush", g_sink.flush());

    bool got = false;
    for (int i = 0; i < 100; ++i)
    {
        if (g_stream.poll())
        {
            got = true;
            break;
        }
        delay(5);
    }
    EXPECT_TRUE("poll/recv", got);

    uint8_t buf[8] = {0};
    const size_t n = g_stream.read(buf, sizeof(buf));
    EXPECT_EQ("read/count", 4L, (long)n);
    EXPECT_EQ("read/b0", 0xDEL, (long)buf[0]);
    EXPECT_EQ("read/b1", 0xADL, (long)buf[1]);
    EXPECT_EQ("read/b2", 0xBEL, (long)buf[2]);
    EXPECT_EQ("read/b3", 0xEFL, (long)buf[3]);
}

void setup()
{
    Serial.begin(115200);
    delay(5000); // ESP32 Serial-USB bridge can drop early output otherwise

    test_loopback();
    test_empty_flush();
    test_chunked_write();

    Serial.print("TEST done ");
    Serial.print(g_pass);
    Serial.print("/");
    Serial.println(g_total);
}

void loop()
{
    delay(1);
}
