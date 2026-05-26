// DUT → Python RTP L16 packet test.
//
// The DUT binds a known port, waits for a "hello" datagram from the
// Python test, learns Python's source address, then emits one RTP L16
// mono packet (PT 11) with a deterministic PCM ramp. Python parses the
// 12-byte header and PCM payload with an independent implementation
// and asserts every field.
//
// Encoder-side cross-implementation check that complements
// rtp_python_loopback/ (Python encodes, DUT decodes).

#include <WiFi.h>
#include <WiFiUdp.h>
#include <PCMFlowUDP.h>

static constexpr uint16_t kKnownPort = 49240;
static constexpr uint32_t kSampleRate = 8000;
static constexpr size_t kFrames = 64;
static constexpr uint32_t kSsrc = 0x13572468;

WiFiUDP g_udp;
RtpSender g_sender(g_udp);

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
    uint8_t scratch[16];
    g_udp.read(scratch, sizeof(scratch));

    // Configure the sender with a fixed SSRC + a known starting timestamp
    // / sequence so the Python side can assert exact values.
    g_sender.begin(peerIp, peerPort, kSsrc);
    g_sender.setFormat({kSampleRate, 1, 16}); // PT 11, L16 mono
    g_sender.setSequenceNumber(0x2222);
    g_sender.setTimestamp(0x10000000);

    static int16_t pcm[kFrames];
    for (size_t i = 0; i < kFrames; ++i)
        pcm[i] = static_cast<int16_t>(static_cast<int>(i) * 100 - 3000);

    g_sender.writeFrames(pcm, kFrames);
    g_sender.flush();

    Serial.println("DUT-TX-OK");
    g_done = true;
}
