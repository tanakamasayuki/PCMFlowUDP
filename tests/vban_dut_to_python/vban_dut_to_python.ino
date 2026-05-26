// DUT → Python VBAN audio packet test.
//
// The DUT binds a known port, waits for a "hello" datagram from the
// Python test (used to discover Python's address), then emits a VBAN
// audio packet containing a deterministic PCM ramp. Python parses the
// resulting bytes with its own independent VBAN decoder and asserts
// every field byte-for-byte.
//
// This is the encoder-side cross-implementation check that complements
// vban_python_loopback/ (Python encodes, DUT decodes). Together they
// rule out symmetric bugs that would slip through the C++-internal
// vban_loopback/ test.

#include <WiFi.h>
#include <WiFiUdp.h>
#include <PCMFlowUDP.h>

static constexpr uint16_t kKnownPort = 49230;
static constexpr uint32_t kSampleRate = 16000;
static constexpr size_t kFrames = 64;
static const char *kStreamName = "DUT-OUT";

WiFiUDP g_udp;
VbanSender g_sender(g_udp);

static IPAddress connectAndReportIp()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    const unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 60000)
        delay(250);
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WIFI_ERROR connect_failed");
        while (true) delay(1000);
    }
    return WiFi.localIP();
}

void setup()
{
    Serial.begin(115200);
    delay(5000); // ESP32 Serial-USB bridge can drop early output otherwise

    const IPAddress localIp = connectAndReportIp();

    if (g_udp.begin(kKnownPort) != 1)
    {
        Serial.println("FAIL udp-begin");
        while (true) delay(1000);
    }
    Serial.print("DUT-READY ip=");
    Serial.print(localIp);
    Serial.print(" port=");
    Serial.println(kKnownPort);
}

static bool g_done = false;

void loop()
{
    if (g_done)
    {
        delay(10);
        return;
    }

    const int n = g_udp.parsePacket();
    if (n <= 0)
    {
        delay(2);
        return;
    }

    const IPAddress peerIp = g_udp.remoteIP();
    const uint16_t peerPort = g_udp.remotePort();
    // Drain the hello datagram so the socket is ready for the next thing.
    uint8_t scratch[16];
    g_udp.read(scratch, sizeof(scratch));

    // Configure the sender to talk back to whoever pinged us, then
    // ship one well-known VBAN audio packet.
    g_sender.begin(peerIp, peerPort, kStreamName);
    g_sender.setFormat({kSampleRate, 1, 16});

    static int16_t pcm[kFrames];
    for (size_t i = 0; i < kFrames; ++i)
        pcm[i] = static_cast<int16_t>(static_cast<int>(i) * 100 - 3000);

    g_sender.writeFrames(pcm, kFrames);
    g_sender.flush(); // 64 < samplesPerPacket(256), force the partial out

    Serial.println("DUT-TX-OK");
    g_done = true;
}
