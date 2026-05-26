// Python → DUT RTP L16 packet test.
//
// On startup the DUT prints `DUT-READY port=<N>`. Python sends a single
// RTP L16-mono packet; the DUT decodes it via RtpReceiver and prints
// the parsed metadata + first three PCM samples, which Python asserts.

#include <WiFiUdp.h>
#include <PCMFlowUDP.h>

static constexpr uint16_t kPort = 49220;

WiFiUDP g_rxUdp;
RtpReceiver g_rx(g_rxUdp);

void setup()
{
    Serial.begin(115200);
    delay(500);
    // Pre-declare format so PCMFlow's sample-rate field is populated
    // before any wire-only sample-rate hint arrives (L16 doesn't carry
    // a sample rate in the header).
    g_rx.setFormat({8000, 1, 16});
    if (!g_rx.begin(kPort))
    {
        Serial.println("FAIL rx-begin");
        while (true) delay(1000);
    }
    Serial.print("DUT-READY port=");
    Serial.println(kPort);
}

static bool g_done = false;

void loop()
{
    if (g_done)
    {
        delay(10);
        return;
    }
    if (!g_rx.poll())
    {
        delay(2);
        return;
    }

    int16_t samples[8] = {0};
    const size_t got = g_rx.readFrames(samples, 8);

    Serial.print("RTP-RX pt=");
    Serial.print(g_rx.payloadType());
    Serial.print(" seq=");
    Serial.print(g_rx.sequenceNumber());
    Serial.print(" ssrc=");
    Serial.print(g_rx.ssrc());
    Serial.print(" frames=");
    Serial.print(got);
    Serial.print(" s0=");
    Serial.print(samples[0]);
    Serial.print(" s1=");
    Serial.print(samples[1]);
    Serial.print(" s2=");
    Serial.println(samples[2]);
    g_done = true;
}
