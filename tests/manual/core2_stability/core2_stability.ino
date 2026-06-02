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
static constexpr uint16_t kRxPort = 49250;
static constexpr uint32_t kSampleRate = 16000;
static constexpr size_t kMaxFrames = 256;
static constexpr unsigned long kStatsIntervalMs = 5000;
static const char *kStreamName = "Stable";

WiFiUDP g_udp;
VbanReceiver g_rx(g_udp);

static uint32_t g_packets = 0;
static uint32_t g_bad = 0;
static uint32_t g_frames = 0;
static uint32_t g_minHeap = 0xFFFFFFFFu;
static unsigned long g_lastStatsMs = 0;

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

static void drawStats()
{
    M5.Display.clear();
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(2);
    M5.Display.println("Stability");
    M5.Display.println();
    M5.Display.print("Packets: ");
    M5.Display.println(g_packets);
    M5.Display.print("Bad: ");
    M5.Display.println(g_bad);
    M5.Display.print("Heap: ");
    M5.Display.println(ESP.getFreeHeap());
    M5.Display.print("RSSI: ");
    M5.Display.println(WiFi.RSSI());
}

static void printStats()
{
    const uint32_t heap = ESP.getFreeHeap();
    if (heap < g_minHeap)
        g_minHeap = heap;

    Serial.print("STABILITY-STAT ms=");
    Serial.print(millis());
    Serial.print(" packets=");
    Serial.print(g_packets);
    Serial.print(" bad=");
    Serial.print(g_bad);
    Serial.print(" frames=");
    Serial.print(g_frames);
    Serial.print(" heap=");
    Serial.print(heap);
    Serial.print(" minHeap=");
    Serial.print(g_minHeap);
    Serial.print(" rssi=");
    Serial.print(WiFi.RSSI());
    Serial.print(" wifi=");
    Serial.println(static_cast<int>(WiFi.status()));
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

    if (!g_rx.begin(kRxPort, kStreamName))
    {
        Serial.println("FAIL rx-begin");
        M5.Display.println("RX begin failed");
        return;
    }

    drawStats();

    Serial.print("DUT-READY ip=");
    Serial.print(localIp);
    Serial.print(" port=");
    Serial.println(kRxPort);
}

void loop()
{
    M5.update();

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WIFI_ERROR disconnected");
        delay(1000);
        return;
    }

    if (g_rx.poll())
    {
        int16_t samples[kMaxFrames] = {0};
        const size_t got = g_rx.readFrames(samples, kMaxFrames);
        const PCMFormat &fmt = g_rx.format();
        if (fmt.sampleRate == kSampleRate && fmt.channels == 1 && got > 0)
        {
            ++g_packets;
            g_frames += got;
        }
        else
        {
            ++g_bad;
        }
    }
    else
    {
        delay(1);
    }

    if (millis() - g_lastStatsMs >= kStatsIntervalMs)
    {
        g_lastStatsMs = millis();
        printStats();
        drawStats();
    }
}
