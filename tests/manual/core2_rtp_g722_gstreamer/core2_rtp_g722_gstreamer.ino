#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <PCMFlowUDP.h>
#include <PCMFlowG722.h>

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

static constexpr unsigned long kWifiTimeoutMs = 60000;
static constexpr uint16_t kRxPort = 5004;
static constexpr uint32_t kSampleRate = 16000;
static constexpr size_t kPayloadBytes = 160;
static constexpr size_t kMaxFrames = 320;

WiFiUDP g_udp;
RtpReceiver g_rx(g_udp);
G722Decoder g_dec;
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

static void drawReady(const IPAddress &ip)
{
    M5.Display.clear();
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(2);
    M5.Display.println("RTP G722");
    M5.Display.println();
    M5.Display.print("IP: ");
    M5.Display.println(ip);
    M5.Display.print("Port: ");
    M5.Display.println(kRxPort);
    M5.Display.println("G722 PT9");
}

static void drawStats(size_t frames)
{
    M5.Display.clear();
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(2);
    M5.Display.println("RTP G722");
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

    if (!g_dec.begin({kSampleRate, 1, 16}))
    {
        Serial.println("FAIL g722-begin");
        M5.Display.println("G722 begin failed");
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

    uint8_t packet[kPayloadBytes] = {0};
    int16_t samples[kMaxFrames] = {0};
    const size_t bytes = g_rx.readEncoded(packet, sizeof(packet));
    const size_t frames = g_dec.decode(packet, bytes, samples, kMaxFrames);
    ++g_packets;

    if (g_rx.payloadType() != static_cast<uint8_t>(pcmflowudp::RtpPayloadType::G722) ||
        bytes == 0 || frames == 0)
    {
        ++g_drops;
    }
    else
    {
        M5.Speaker.playRaw(samples, frames, kSampleRate, false, 1, 0, false);
    }

    Serial.print("RTP-G722-RX pt=");
    Serial.print(g_rx.payloadType());
    Serial.print(" seq=");
    Serial.print(g_rx.sequenceNumber());
    Serial.print(" ssrc=");
    Serial.print(g_rx.ssrc());
    Serial.print(" bytes=");
    Serial.print(bytes);
    Serial.print(" frames=");
    Serial.print(frames);
    Serial.print(" packets=");
    Serial.print(g_packets);
    Serial.print(" drops=");
    Serial.println(g_drops);

    drawStats(frames);
}
