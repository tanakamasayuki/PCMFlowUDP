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
static constexpr uint16_t kRxPort = 5004;
static constexpr uint32_t kSampleRate = 16000;
static constexpr size_t kPacketFrames = 320;
static constexpr size_t kMaxPresetPackets = 4;
static constexpr size_t kMaxPlayFrames = kPacketFrames * kMaxPresetPackets;
static constexpr unsigned long kStatsIntervalMs = 5000;
static constexpr uint8_t kGstreamerL16PayloadType = 96;
using Player = M5SpeakerBufferedPlayer<kMaxPlayFrames>;

struct Preset
{
    const char *name;
    size_t initialPackets;
    size_t chunkPackets;
};

static constexpr Preset kPresets[] = {
    {"p0-low", 1, 1},
    {"p1-voip", 2, 1},
    {"p2-balanced", 2, 2},
    {"p3-safe", 3, 2},
    {"p4-stable", 4, 2},
};
static constexpr size_t kPresetCount = sizeof(kPresets) / sizeof(kPresets[0]);

WiFiUDP g_udp;
RtpReceiver g_rx(g_udp);
Player g_player;
static size_t g_presetIndex = 2;
static uint32_t g_packets = 0;
static uint32_t g_drops = 0;
static uint32_t g_readEmpty = 0;
static uint32_t g_resets = 0;
static uint32_t g_lastStatsGapRisks = 0;
static unsigned long g_lastStatsMs = 0;

static const Preset &preset()
{
    return kPresets[g_presetIndex];
}

static const char *stateName()
{
    return g_player.hasStarted() ? "RUNNING" : "PREFILLING";
}

static size_t initialFrames()
{
    return preset().initialPackets * kPacketFrames;
}

static size_t chunkFrames()
{
    return preset().chunkPackets * kPacketFrames;
}

static size_t targetFrames()
{
    return g_player.targetFrames();
}

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

static void resetForPreset(bool countReset)
{
    M5.Speaker.stop();
    g_rx.end();
    g_rx.setFormat({kSampleRate, 1, 16});
    g_rx.setDynamicL16PayloadType(kGstreamerL16PayloadType, 1);
    g_rx.begin(kRxPort);
    g_player.begin({kSampleRate, 1, 16},
                   {static_cast<uint16_t>(preset().initialPackets * 20u),
                    static_cast<uint16_t>(preset().chunkPackets * 20u)});

    g_packets = 0;
    g_drops = 0;
    g_readEmpty = 0;
    g_lastStatsGapRisks = 0;
    if (countReset)
        ++g_resets;

    Serial.print("BUFFER-TUNE preset-change name=");
    Serial.print(preset().name);
    Serial.print(" initial=");
    Serial.print(preset().initialPackets);
    Serial.print(" chunk=");
    Serial.print(preset().chunkPackets);
    Serial.print(" resets=");
    Serial.println(g_resets);
}

static void drawReady(const IPAddress &ip)
{
    M5.Display.clear();
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(2);
    M5.Display.println("RTP buffer tune");
    M5.Display.println();
    M5.Display.print("IP: ");
    M5.Display.println(ip);
    M5.Display.print("Port: ");
    M5.Display.println(kRxPort);
    M5.Display.println("A next B prev C reset");
}

static void drawStats()
{
    M5.Display.clear();
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(2);
    M5.Display.println("RTP buffer tune");
    M5.Display.print(preset().name);
    M5.Display.print(" ");
    M5.Display.println(stateName());
    M5.Display.print("init/chunk ");
    M5.Display.print(preset().initialPackets);
    M5.Display.print("/");
    M5.Display.println(preset().chunkPackets);
    M5.Display.print("pkt/drop ");
    M5.Display.print(g_packets);
    M5.Display.print("/");
    M5.Display.println(g_drops);
    M5.Display.print("empty/wait ");
    M5.Display.print(g_readEmpty);
    M5.Display.print("/");
    M5.Display.println(g_player.waits());
    M5.Display.print("chunks ");
    M5.Display.println(g_player.chunks());
}

static void printStats(size_t got)
{
    const uint32_t lateDelta = g_player.gapRisks() - g_lastStatsGapRisks;
    g_lastStatsGapRisks = g_player.gapRisks();

    Serial.print("TUNE ");
    Serial.print(" preset=");
    Serial.print(preset().name);
    Serial.print(" state=");
    Serial.print(stateName());
    Serial.print(" init=");
    Serial.print(preset().initialPackets);
    Serial.print(" chunk=");
    Serial.print(preset().chunkPackets);
    Serial.print(" pkt=");
    Serial.print(g_packets);
    Serial.print(" drop=");
    Serial.print(g_drops);
    Serial.print(" empty=");
    Serial.print(g_readEmpty);
    Serial.print(" wait=");
    Serial.print(g_player.waits());
    Serial.print(" chunks=");
    Serial.print(g_player.chunks());
    Serial.print(" late=");
    Serial.print(g_player.gapRisks());
    Serial.print(" late_delta=");
    Serial.print(lateDelta);
    Serial.print(" resets=");
    Serial.println(g_resets);
}

static void handleButtons()
{
    if (M5.BtnA.wasClicked())
    {
        g_presetIndex = (g_presetIndex + 1) % kPresetCount;
        resetForPreset(true);
    }
    else if (M5.BtnB.wasClicked())
    {
        g_presetIndex = (g_presetIndex + kPresetCount - 1) % kPresetCount;
        resetForPreset(true);
    }
    else if (M5.BtnC.wasClicked())
    {
        resetForPreset(true);
    }
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

    g_rx.setFormat({kSampleRate, 1, 16});
    g_rx.setDynamicL16PayloadType(kGstreamerL16PayloadType, 1);
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
    Serial.println(kRxPort);
    resetForPreset(false);
}

void loop()
{
    M5.update();
    handleButtons();

    size_t got = 0;
    if (g_rx.poll())
    {
        int16_t *samples = g_player.writableData();
        const size_t room = g_player.writableFrames();
        got = g_rx.readFrames(samples, room);
        ++g_packets;

        if (g_rx.payloadType() != kGstreamerL16PayloadType || got == 0)
        {
            ++g_drops;
            if (g_player.hasStarted())
                ++g_readEmpty;
        }
        else
        {
            g_player.commitFrames(got);
        }
    }

    const unsigned long now = millis();
    if (now - g_lastStatsMs >= kStatsIntervalMs)
    {
        g_lastStatsMs = now;
        if (g_player.hasStarted() || got != 0 || g_player.fillFrames() != 0)
            printStats(got);
        drawStats();
    }

    if (got == 0)
        delay(1);
}
