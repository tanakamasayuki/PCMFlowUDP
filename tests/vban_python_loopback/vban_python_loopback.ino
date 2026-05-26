// Python → DUT VBAN audio packet test.
//
// On startup the DUT prints `DUT-READY port=<N>` so Python can discover
// the local port. Then VbanReceiver pulls one VBAN audio packet and the
// DUT prints its decoded metadata + the first three PCM samples — those
// lines are what the Python test asserts against.

#include <WiFiUdp.h>
#include <PCMFlowUDP.h>

static constexpr uint16_t kPort = 49210;

WiFiUDP g_rxUdp;
VbanReceiver g_rx(g_rxUdp);

void setup()
{
    Serial.begin(115200);
    delay(500);

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

    Serial.print("VBAN-RX rate=");
    Serial.print(g_rx.format().sampleRate);
    Serial.print(" channels=");
    Serial.print(g_rx.format().channels);
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
