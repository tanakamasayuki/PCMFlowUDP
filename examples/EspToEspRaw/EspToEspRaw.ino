// PCMFlowUDP example: EspToEspRaw
//
// Two ESP32 boards on the same LAN exchange raw PCM16 audio over plain
// UDP. No protocol header on the wire — each UDP datagram is one chunk
// of PCM samples in host byte order. Lowest overhead, useful for
// proprietary device-to-device links where you control both ends.
//
// The same firmware runs on both boards. Each board:
//
//   mic   → RawUdpSink     (peer IP, fixed port)
//   peer  → RawUdpStream   (this board's port) → PCMFlow → DAC
//
// To use, flash this on two boards and set the peer IPs in
// kPeerIp / kLocalPort accordingly (or use the broadcast IP
// 255.255.255.255 to skip configuration entirely).
//
// TODO: fill in the I2S microphone / speaker bring-up for the target
// hardware (M5Stack Core2, INMP441 + MAX98357A, etc.).

#include <WiFi.h>
#include <WiFiUdp.h>
#include <PCMFlow.h>
#include <PCMFlowUDP.h>

static const char *kWifiSsid = "your-ssid";
static const char *kWifiPass = "your-passphrase";

// LAN broadcast: every board on the same /24 receives this. Replace
// with a peer's IP if you have only two boards and want point-to-point.
static const IPAddress kPeerIp(255, 255, 255, 255);
static constexpr uint16_t kPort = 49000;

static constexpr uint32_t kSampleRate = 16000;
static constexpr size_t kFramesPerChunk = 160; // 10 ms at 16 kHz

WiFiUDP g_udp;
RawUdpSink g_sink(g_udp);
RawUdpStream g_stream(g_udp);

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("PCMFlowUDP EspToEspRaw starting");

    WiFi.mode(WIFI_STA);
    WiFi.begin(kWifiSsid, kWifiPass);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(200);
        Serial.print('.');
    }
    Serial.print("\nLocal IP: ");
    Serial.println(WiFi.localIP());

    // Single shared UDP socket: bound for receive, also used for
    // transmit. Send to broadcast on the same port so both ESP32s
    // hear each other.
    if (g_udp.begin(kPort) != 1)
    {
        Serial.println("WiFiUDP.begin failed");
        while (true)
            delay(1000);
    }

    if (!g_stream.begin(kPort))
    {
        Serial.println("RawUdpStream.begin failed");
        while (true)
            delay(1000);
    }
    if (!g_sink.begin(kPeerIp, kPort))
    {
        Serial.println("RawUdpSink.begin failed");
        while (true)
            delay(1000);
    }

    Serial.println("Ready");
}

void loop()
{
    // ---- TX: mic → RAW UDP -----------------------------------------
    int16_t mic[kFramesPerChunk];
    // TODO: replace with real microphone capture (I2S.read()).
    for (size_t i = 0; i < kFramesPerChunk; ++i)
        mic[i] = 0;
    g_sink.write(mic, sizeof(mic));
    g_sink.flush(); // one datagram per chunk

    // ---- RX: RAW UDP → speaker -------------------------------------
    if (g_stream.poll())
    {
        // Filter out our own broadcast (we receive what we sent).
        if (g_stream.remoteIP() == WiFi.localIP())
            return;

        int16_t spk[kFramesPerChunk];
        const size_t got = g_stream.read(spk, sizeof(spk));
        // TODO: hand `spk` to your I2S DAC for playback. The number of
        // bytes is `got`; samples = got / sizeof(int16_t).
        (void)got;
    }
}
