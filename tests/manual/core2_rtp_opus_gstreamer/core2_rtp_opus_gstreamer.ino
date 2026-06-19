#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <PCMFlowUDP.h>
#include <PCMFlowOpus.h>
#include <PCMFlowDeviceM5.h>

SET_LOOP_TASK_STACK_SIZE(16 * 1024);

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

static constexpr unsigned long kWifiTimeoutMs = 60000;
static constexpr uint16_t kRxPort = 5004;
static constexpr uint8_t kPayloadType = 96;
static constexpr uint32_t kSampleRate = 48000;
static constexpr size_t kMaxPacketBytes = 400;
static constexpr size_t kPacketFrames = 960; // 20 ms at 48 kHz
static constexpr size_t kMaxPlayFrames = kPacketFrames * 8;
static constexpr unsigned long kStatsIntervalMs = 500;
using Player = M5SpeakerBufferedPlayer<kMaxPlayFrames>;

WiFiUDP g_udp;
RtpReceiver g_rx(g_udp);
OpusDecoder g_dec;
Player g_player;
static uint8_t g_packet[kMaxPacketBytes] = {};
static uint32_t g_packets = 0;
static uint32_t g_drops = 0;
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

static void drawReady(const IPAddress &ip)
{
    M5.Display.clear();
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(2);
    M5.Display.println("RTP Opus");
    M5.Display.println();
    M5.Display.print("IP: ");
    M5.Display.println(ip);
    M5.Display.print("Port: ");
    M5.Display.println(kRxPort);
    M5.Display.println("Opus PT96 160/80");
}

static void drawStats(size_t frames)
{
    M5.Display.clear();
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(2);
    M5.Display.println("RTP Opus");
    M5.Display.println();
    M5.Display.print("Packets: ");
    M5.Display.println(g_packets);
    M5.Display.print("PT: ");
    M5.Display.println(g_rx.payloadType());
    M5.Display.print("Frames: ");
    M5.Display.println(frames);
    M5.Display.print("Drops: ");
    M5.Display.println(g_drops);
    M5.Display.print("Heap: ");
    M5.Display.println(ESP.getFreeHeap());
    M5.Display.print("Waits: ");
    M5.Display.println(g_player.waits());
    M5.Display.print("Gaps: ");
    M5.Display.println(g_player.gapRisks());
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

    if (!g_player.begin({kSampleRate, 1, 16}, {160, 80}))
    {
        Serial.println("FAIL player-begin");
        M5.Display.println("Player begin failed");
        return;
    }

    if (!g_dec.begin({kSampleRate, 1, 16}))
    {
        Serial.println("FAIL opus-begin");
        M5.Display.println("Opus begin failed");
        return;
    }

    if (!g_rx.begin(kRxPort))
    {
        Serial.println("FAIL rx-begin");
        M5.Display.println("RX begin failed");
        return;
    }

    drawReady(localIp);

    Serial.print("DUT-READY ip=");
    Serial.print(localIp);
    Serial.print(" port=");
    Serial.print(kRxPort);
    Serial.print(" heap=");
    Serial.println(ESP.getFreeHeap());
}

void loop()
{
    M5.update();

    if (!g_rx.poll())
    {
        delay(2);
        return;
    }

    const size_t bytes = g_rx.readEncoded(g_packet, sizeof(g_packet));
    int16_t *samples = g_player.writableData();
    const size_t room = g_player.writableFrames();
    const size_t frames = g_dec.decodePacket(g_packet, bytes, samples, room);
    ++g_packets;

    if (g_rx.payloadType() != kPayloadType || bytes == 0 || frames == 0)
    {
        ++g_drops;
    }
    else
    {
        g_player.commitFrames(frames);
    }

    const unsigned long now = millis();
    if (now - g_lastStatsMs >= kStatsIntervalMs)
    {
        g_lastStatsMs = now;

        Serial.print("RTP-OPUS-RX pt=");
        Serial.print(g_rx.payloadType());
        Serial.print(" seq=");
        Serial.print(g_rx.sequenceNumber());
        Serial.print(" ssrc=");
        Serial.print(g_rx.ssrc());
        Serial.print(" bytes=");
        Serial.print(bytes);
        Serial.print(" frames=");
        Serial.print(frames);
        Serial.print(" fill=");
        Serial.print(g_player.fillFrames());
        Serial.print(" packets=");
        Serial.print(g_packets);
        Serial.print(" drops=");
        Serial.print(g_drops);
        Serial.print(" waits=");
        Serial.print(g_player.waits());
        Serial.print(" chunks=");
        Serial.print(g_player.chunks());
        Serial.print(" gaps=");
        Serial.print(g_player.gapRisks());
        Serial.print(" heap=");
        Serial.println(ESP.getFreeHeap());

        drawStats(frames);
    }
}
