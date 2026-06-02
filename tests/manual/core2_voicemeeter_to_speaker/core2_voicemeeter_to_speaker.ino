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
static constexpr uint16_t kVbanPort = 6980;
static constexpr uint32_t kExpectedRate = 16000;
static constexpr size_t kMaxFrames = 256;

WiFiUDP g_udp;
VbanReceiver g_rx(g_udp);
static uint32_t g_packets = 0;
static uint32_t g_drops = 0;

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

static void drawStatus(const IPAddress &ip, size_t frames = 0)
{
    M5.Display.clear();
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(2);
    M5.Display.println("Voicemeeter -> Core2");
    M5.Display.println();
    M5.Display.print("IP: ");
    M5.Display.println(ip);
    M5.Display.print("Port: ");
    M5.Display.println(kVbanPort);
    M5.Display.print("RX: ");
    M5.Display.println(g_packets);
    M5.Display.print("Frames: ");
    M5.Display.println(frames);
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

    M5.Speaker.begin();
    M5.Speaker.setVolume(160);

    if (!g_rx.begin(kVbanPort))
    {
        Serial.println("FAIL rx-begin");
        M5.Display.println("RX begin failed");
        return;
    }

    drawStatus(localIp);

    Serial.print("DUT-READY ip=");
    Serial.print(localIp);
    Serial.print(" port=");
    Serial.print(kVbanPort);
    Serial.println(" stream=PcToCore2");
}

void loop()
{
    M5.update();

    if (!g_rx.poll())
    {
        delay(2);
        return;
    }

    int16_t samples[kMaxFrames] = {0};
    const size_t got = g_rx.readFrames(samples, kMaxFrames);
    const PCMFormat &fmt = g_rx.format();
    ++g_packets;

    if (fmt.sampleRate != kExpectedRate || fmt.channels != 1 || got == 0)
    {
        ++g_drops;
    }
    else
    {
        M5.Speaker.playRaw(samples, got, fmt.sampleRate, false, 1, 0, false);
    }

    Serial.print("VBAN-RX stream=PcToCore2 rate=");
    Serial.print(fmt.sampleRate);
    Serial.print(" channels=");
    Serial.print(fmt.channels);
    Serial.print(" frames=");
    Serial.print(got);
    Serial.print(" packets=");
    Serial.print(g_packets);
    Serial.print(" drops=");
    Serial.println(g_drops);

    drawStatus(WiFi.localIP(), got);
}
