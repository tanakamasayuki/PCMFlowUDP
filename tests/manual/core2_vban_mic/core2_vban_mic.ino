#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <PCMFlowUDP.h>

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

static constexpr unsigned long kWifiTimeoutMs = 60000;
static constexpr uint16_t kControlPort = 49241;
static constexpr uint32_t kSampleRate = 16000;
static constexpr size_t kFrames = 256;
static constexpr size_t kPacketsToSend = 320;
static const char *kStreamName = "Core2Mic";

WiFiUDP g_udp;
VbanSender g_sender(g_udp);
static bool g_streaming = false;
static size_t g_packets = 0;

static bool connectWifi(IPAddress &ip)
{
    if (String(WIFI_SSID).isEmpty())
    {
        Serial.println("WIFI_ERROR missing_ssid");
        return false;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    const unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < kWifiTimeoutMs)
        delay(250);

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.print("WIFI_ERROR connect_failed ");
        Serial.println(static_cast<int>(WiFi.status()));
        return false;
    }

    ip = WiFi.localIP();
    return true;
}

static void drawReady(const IPAddress &ip)
{
    M5.Display.clear();
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(2);
    M5.Display.println("VBAN mic");
    M5.Display.println();
    M5.Display.print("IP: ");
    M5.Display.println(ip);
    M5.Display.print("Port: ");
    M5.Display.println(kControlPort);
    M5.Display.println("Waiting...");
}

static void drawTx()
{
    M5.Display.clear();
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(2);
    M5.Display.println("VBAN mic");
    M5.Display.println();
    M5.Display.print("TX packets: ");
    M5.Display.println(g_packets);
    M5.Display.print("Rate: ");
    M5.Display.println(kSampleRate);
    M5.Display.print("Frames: ");
    M5.Display.println(kFrames);
}

void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);

    Serial.begin(115200);
    delay(5000);

    M5.Display.setTextSize(2);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Connecting WiFi...");

    IPAddress localIp;
    if (!connectWifi(localIp))
    {
        M5.Display.println("WiFi failed");
        return;
    }

    if (g_udp.begin(kControlPort) != 1)
    {
        Serial.println("FAIL udp-begin");
        M5.Display.println("UDP begin failed");
        return;
    }

    M5.Speaker.end();
    if (!M5.Mic.begin())
    {
        Serial.println("FAIL mic-begin");
        M5.Display.println("Mic begin failed");
        return;
    }

    drawReady(localIp);

    Serial.print("DUT-READY ip=");
    Serial.print(localIp);
    Serial.print(" port=");
    Serial.println(kControlPort);
}

void loop()
{
    M5.update();

    if (!g_streaming)
    {
        if (g_udp.parsePacket() <= 0)
        {
            delay(2);
            return;
        }

        const IPAddress peerIp = g_udp.remoteIP();
        const uint16_t peerPort = g_udp.remotePort();
        uint8_t scratch[32];
        g_udp.read(scratch, sizeof(scratch));

        if (!g_sender.begin(peerIp, peerPort, kStreamName) ||
            !g_sender.setFormat({kSampleRate, 1, 16}))
        {
            Serial.println("FAIL sender-begin");
            return;
        }

        Serial.print("VBAN-TX stream=");
        Serial.print(kStreamName);
        Serial.print(" rate=");
        Serial.print(kSampleRate);
        Serial.print(" channels=1 dest=");
        Serial.print(peerIp);
        Serial.print(":");
        Serial.println(peerPort);
        g_streaming = true;
    }

    if (g_packets >= kPacketsToSend)
    {
        delay(100);
        return;
    }

    int16_t samples[kFrames] = {0};
    if (!M5.Mic.record(samples, kFrames, kSampleRate))
    {
        delay(1);
        return;
    }

    const size_t written = g_sender.writeFrames(samples, kFrames);
    g_sender.flush();
    ++g_packets;

    Serial.print("VBAN-TX packets=");
    Serial.print(g_packets);
    Serial.print(" frames=");
    Serial.print(written);
    Serial.print(" s0=");
    Serial.print(samples[0]);
    Serial.print(" s1=");
    Serial.print(samples[1]);
    Serial.print(" s2=");
    Serial.println(samples[2]);

    drawTx();
}
