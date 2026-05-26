// PCMFlowUDP example: RtpVoipG711
//
// Streams microphone audio from an ESP32 to a PC SIP softphone (or
// gst-launch / Wireshark) as an RTP/PCMU flow, RFC 3550 + RFC 3551 §4.5.14.
//
//   8 kHz mono mic → G711Encoder (mu-law) → RtpSender::writeEncoded()
//
// The receive direction (PC → ESP32) mirrors:
//
//   RtpReceiver::readEncoded() → G711Decoder → PCMFlow → I2S DAC
//
// Both directions share one WiFiUDP instance bound to the local port,
// matching the typical RTP socket convention (one symmetric port per
// flow).
//
// Verify on the PC side with either:
//
//   gst-launch-1.0 -v udpsrc port=5004 caps="application/x-rtp,media=audio,\
//     encoding-name=PCMU,clock-rate=8000,payload=0" ! rtppcmudepay ! \
//     mulawdec ! audioconvert ! autoaudiosink
//
//   ...or any standards-compliant SIP softphone with the SDP offer set
//   to PCMU on the configured port.
//
// TODO: fill in the I2S microphone and speaker code for the target
// board (M5 mic / INMP441, MAX98357A speaker, etc.). The structure
// below shows the PCMFlowUDP-facing wiring; the codec wiring requires
// the sibling library PCMFlowG711 to be installed alongside.

#include <WiFi.h>
#include <WiFiUdp.h>
#include <PCMFlow.h>
#include <PCMFlowUDP.h>
#include <PCMFlowG711.h>

static const char *kWifiSsid = "your-ssid";
static const char *kWifiPass = "your-passphrase";

// RTP peer (the PC running gst / softphone) and ports.
static const IPAddress kRemoteIp(192, 168, 1, 100);
static const uint16_t kRemotePort = 5004;
static const uint16_t kLocalPort = 5004;

// G.711 / PCMU framing constants.
static constexpr uint32_t kSampleRate = 8000;
static constexpr size_t kSamplesPerPacket = 160; // 20 ms at 8 kHz
static constexpr uint32_t kSsrc = 0x12345678;    // pick anything stable

WiFiUDP g_udp;
RtpSender g_sender(g_udp);
RtpReceiver g_receiver(g_udp);

G711Encoder g_enc;
G711Decoder g_dec;
PCMFlow g_audio;

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("PCMFlowUDP RtpVoipG711 starting");

    WiFi.mode(WIFI_STA);
    WiFi.begin(kWifiSsid, kWifiPass);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(200);
        Serial.print('.');
    }
    Serial.print("\nLocal IP: ");
    Serial.println(WiFi.localIP());

    if (g_udp.begin(kLocalPort) != 1)
    {
        Serial.println("WiFiUDP.begin failed");
        while (true)
            delay(1000);
    }

    // RTP sender: PCMU (PT 0) at 8 kHz, 20 ms timestamp increment.
    if (!g_sender.begin(kRemoteIp, kRemotePort, kSsrc))
    {
        Serial.println("RtpSender.begin failed");
        while (true)
            delay(1000);
    }
    g_sender.setPayloadType(
        static_cast<uint8_t>(pcmflowudp::RtpPayloadType::PCMU), kSampleRate);
    g_sender.setTimestampIncrement(static_cast<uint32_t>(kSamplesPerPacket));

    // Codec: G.711 mu-law.
    g_enc.begin({kSampleRate, 1, 16}, G711Variant::MuLaw);
    g_dec.begin({kSampleRate, 1, 16}, G711Variant::MuLaw);

    // Playback pipeline: G711Decoder is a PCMSource; PCMFlow consumes
    // it and pushes to the I2S DAC. Application sets up the DAC.
    g_audio.setOutputFormat({kSampleRate, 1, 16});
    g_audio.setInputSource(g_dec);

    Serial.println("Ready");
}

void loop()
{
    // ---- TX: mic → encoder → RTP -----------------------------------
    int16_t mic[kSamplesPerPacket];
    // TODO: replace with real microphone capture (e.g. I2S.read()).
    for (size_t i = 0; i < kSamplesPerPacket; ++i)
        mic[i] = 0;

    uint8_t encoded[kSamplesPerPacket];
    const size_t n =
        g_enc.encode(mic, kSamplesPerPacket, encoded, sizeof(encoded));
    if (n > 0)
        g_sender.writeEncoded(encoded, n);

    // ---- RX: RTP → decoder → PCMFlow -------------------------------
    if (g_receiver.poll() && !g_receiver.isPcm())
    {
        uint8_t buf[kSamplesPerPacket];
        const size_t got = g_receiver.readEncoded(buf, sizeof(buf));
        if (got > 0)
        {
            // Feed the decoder; PCMFlow::pump() (called below) pulls
            // PCM samples and drives the speaker.
            g_dec.decode(buf, got, nullptr, 0);
        }
    }

    g_audio.pump();
}
