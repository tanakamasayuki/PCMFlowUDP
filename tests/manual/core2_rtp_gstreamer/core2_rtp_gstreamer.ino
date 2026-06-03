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
static constexpr uint16_t kRxPort = 5004;
static constexpr uint32_t kSampleRate = 16000;
static constexpr size_t kMaxFrames = 320;
static constexpr size_t kMaxPlayFrames = kMaxFrames * 4;
static constexpr size_t kAudioBuffers = 3;
static constexpr size_t kRxPcmBufferBytes = 4096;
static constexpr unsigned long kStatsIntervalMs = 500;
static constexpr uint8_t kGstreamerL16PayloadType = 96;

WiFiUDP g_udp;
RtpReceiver g_rx(g_udp);
static uint8_t g_rxPcmBuffer[kRxPcmBufferBytes] = {};
static uint32_t g_packets = 0;
static uint32_t g_drops = 0;
static unsigned long g_lastStatsMs = 0;
static int16_t g_audio[kAudioBuffers][kMaxPlayFrames] = {};
static size_t g_playFrames = kMaxFrames * 2;
static size_t g_initialPlayFrames = kMaxFrames * 2;
static size_t g_audioIndex = 0;
static size_t g_audioFill = 0;
static bool g_playStarted = false;

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
    M5.Display.println("GStreamer RTP");
    M5.Display.println();
    M5.Display.print("IP: ");
    M5.Display.println(ip);
    M5.Display.print("Port: ");
    M5.Display.println(kRxPort);
    M5.Display.println("L16 16k mono PT11");
}

static void drawStats(size_t frames)
{
    M5.Display.clear();
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(2);
    M5.Display.println("GStreamer RTP");
    M5.Display.println();
    M5.Display.print("Packets: ");
    M5.Display.println(g_packets);
    M5.Display.print("PT: ");
    M5.Display.println(g_rx.payloadType());
    M5.Display.print("Frames: ");
    M5.Display.println(frames);
    M5.Display.print("Drops: ");
    M5.Display.println(g_drops);
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
    g_rx.setPcmBuffer(g_rxPcmBuffer, sizeof(g_rxPcmBuffer));
    const RtpReceiver::PcmBufferProfile profile = RtpReceiver::hardwareSpeakerPcmBuffer();
    g_initialPlayFrames = g_rx.framesForMs(profile.initialPrebufferMs);
    g_playFrames = g_rx.framesForMs(profile.readChunkMs);
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
}

void loop()
{
    M5.update();

    if (!g_rx.poll())
    {
        delay(2);
        return;
    }

    int16_t *samples = g_audio[g_audioIndex] + g_audioFill;
    const size_t targetFrames = g_playStarted ? g_playFrames : g_initialPlayFrames;
    const size_t room = targetFrames - g_audioFill;
    const size_t got = g_rx.readFrames(samples, room);
    ++g_packets;

    if (g_rx.payloadType() != kGstreamerL16PayloadType || got == 0)
    {
        ++g_drops;
    }
    else
    {
        g_audioFill += got;
        if (g_audioFill >= targetFrames)
        {
            while (!M5.Speaker.playRaw(g_audio[g_audioIndex], g_audioFill, kSampleRate, false, 1, 0, false))
            {
                delay(1);
            }
            g_audioIndex = (g_audioIndex + 1) % kAudioBuffers;
            g_audioFill = 0;
            g_playStarted = true;
        }
    }

    const unsigned long now = millis();
    if (now - g_lastStatsMs >= kStatsIntervalMs)
    {
        g_lastStatsMs = now;

        Serial.print("RTP-GST-RX pt=");
        Serial.print(g_rx.payloadType());
        Serial.print(" seq=");
        Serial.print(g_rx.sequenceNumber());
        Serial.print(" ssrc=");
        Serial.print(g_rx.ssrc());
        Serial.print(" frames=");
        Serial.print(got);
        Serial.print(" packets=");
        Serial.print(g_packets);
        Serial.print(" drops=");
        Serial.println(g_drops);

        drawStats(got);
    }
}
