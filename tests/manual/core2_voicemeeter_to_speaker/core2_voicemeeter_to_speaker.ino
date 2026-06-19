#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <PCMFlowDeviceM5.h>
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
static constexpr size_t kInitialPlayFrames = (kExpectedRate * 80u) / 1000u;
static constexpr unsigned long kStatsIntervalMs = 500;
static const char *kStreamName = "PcToCore2";
using Player = M5SpeakerBufferedPlayer<kInitialPlayFrames>;

WiFiUDP g_udp;
VbanReceiver g_rx(g_udp);
Player g_player;
static uint32_t g_packets = 0;
static uint32_t g_drops = 0;
static unsigned long g_lastStatsMs = 0;
static size_t g_lastFrames = 0;
static uint32_t g_lastLoggedPackets = 0;

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
    M5.Display.print("Waits: ");
    M5.Display.println(g_player.waits());
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

    if (!g_player.begin({kExpectedRate, 1, 16}, {80, 64}))
    {
        Serial.println("FAIL player-begin");
        M5.Display.println("Player begin failed");
        return;
    }

    if (!g_rx.begin(kVbanPort, kStreamName))
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
    Serial.print(" stream=");
    Serial.println(kStreamName);
}

static void printStats(const PCMFormat &fmt)
{
    Serial.print("VBAN-RX stream=");
    Serial.print(g_rx.currentStreamName());
    Serial.print(" rate=");
    Serial.print(fmt.sampleRate);
    Serial.print(" channels=");
    Serial.print(fmt.channels);
    Serial.print(" frames=");
    Serial.print(g_lastFrames);
    Serial.print(" packets=");
    Serial.print(g_packets);
    Serial.print(" drops=");
    Serial.print(g_drops);
    Serial.print(" waits=");
    Serial.print(g_player.waits());
    Serial.print(" chunks=");
    Serial.print(g_player.chunks());
    Serial.print(" gaps=");
    Serial.println(g_player.gapRisks());
}

void loop()
{
    M5.update();

    size_t got = 0;
    if (g_rx.poll())
    {
        const PCMFormat &fmt = g_rx.format();
        ++g_packets;

        if (fmt.sampleRate != kExpectedRate || fmt.channels != 1)
        {
            ++g_drops;
        }
        else
        {
            int16_t *samples = g_player.writableData();
            const size_t room = g_player.writableFrames();
            got = g_rx.readFrames(samples, room);
            if (got == 0)
            {
                ++g_drops;
            }
            else
            {
                g_lastFrames = got;
                g_player.commitFrames(got);
            }
        }
    }
    else
    {
        delay(1);
    }

    const unsigned long now = millis();
    if (now - g_lastStatsMs >= kStatsIntervalMs)
    {
        g_lastStatsMs = now;
        const PCMFormat &fmt = g_rx.format();
        if (fmt.sampleRate != 0 && g_packets != g_lastLoggedPackets)
        {
            g_lastLoggedPackets = g_packets;
            printStats(fmt);
            drawStatus(WiFi.localIP(), g_lastFrames);
        }
    }
}
