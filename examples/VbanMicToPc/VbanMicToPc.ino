// en: Send PCM packets using the VBAN format
// ja: VBAN形式でPCMのPacketを送信する
#include <WiFi.h>
#include <WiFiUdp.h>
#include <PCMFlow.h>
#include <PCMFlowUDP.h>

static const char *kWifiSsid = "your-ssid";
static const char *kWifiPass = "your-passphrase";

// VBAN destination — replace with the LAN IP of the PC running
// Voicemeeter / VBAN Receptor, or use 255.255.255.255 for broadcast.
static const IPAddress kDestIp(192, 168, 1, 100);
static const uint16_t kDestPort = 6980;       // VBAN default
static const char *kStreamName = "ESP32-Mic"; // shown in VBAN Receptor

static const uint32_t kSampleRate = 16000;
static const size_t kFramesPerChunk = 256; // 16 ms at 16 kHz

WiFiUDP g_udp;
VbanSender g_sender(g_udp);

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("PCMFlowUDP VbanMicToPc starting");

    WiFi.mode(WIFI_STA);
    WiFi.begin(kWifiSsid, kWifiPass);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(200);
        Serial.print('.');
    }
    Serial.print("\nLocal IP: ");
    Serial.println(WiFi.localIP());

    // Bind an ephemeral local UDP socket before sending. ESP32's
    // WiFiUDP opens lazily, but EthernetUDP / WiFiNINA / WiFiS3 /
    // lang-ship:host all require this explicit call — writing it
    // unconditionally keeps the sketch portable across cores.
    // en: Initialize this component after its prerequisites; inspect the returned status where provided.
    // ja: 前提となる機器を準備してから初期化し、結果を返すAPIでは成否を確認します。
    if (g_udp.begin(0) != 1)
    {
        Serial.println("WiFiUDP.begin failed");
        while (true)
            delay(1000);
    }

    if (!g_sender.begin(kDestIp, kDestPort, kStreamName))
    {
        Serial.println("VbanSender.begin failed");
        while (true)
            delay(1000);
    }
    g_sender.setFormat({kSampleRate, 1, 16});

    Serial.println("Streaming");
}

void loop()
{
    int16_t mic[kFramesPerChunk];

    // TODO: replace this with a real microphone capture for your board
    // (e.g. I2S.read() into `mic`). The placeholder below emits silence
    // at a steady cadence so the wiring can be observed in VBAN
    // Receptor while bringing the hardware up.
    for (size_t i = 0; i < kFramesPerChunk; ++i)
        mic[i] = 0;

    // en: Check the number accepted and retain any unconsumed tail; a frame is not a byte.
    // ja: 受理量を確認し、残りを保持します。フレーム数とByte数は異なります。
    g_sender.writeFrames(mic, kFramesPerChunk);
}
