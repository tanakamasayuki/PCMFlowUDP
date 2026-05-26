// Python ↔ DUT bidirectional RAW UDP loopback.
//
// On startup the DUT prints `DUT-READY port=<N>` so the Python test
// can discover the local port. Then it polls for one incoming
// datagram, echoes it back to the sender, and prints `RX <N> bytes`
// so the Python side can observe the receive over Serial as well.

#include <WiFiUdp.h>
#include <PCMFlowUDP.h>

static constexpr uint16_t kRxPort = 49200;

WiFiUDP g_rxUdp;
WiFiUDP g_txUdp;
RawUdpStream g_stream(g_rxUdp);

void setup()
{
    Serial.begin(115200);
    delay(500);

    // TX side needs an ephemeral local bind so endPacket can sendto.
    if (g_txUdp.begin(0) != 1)
    {
        Serial.println("FAIL tx-begin");
        while (true) delay(1000);
    }
    if (!g_stream.begin(kRxPort))
    {
        Serial.println("FAIL stream-begin");
        while (true) delay(1000);
    }

    Serial.print("DUT-READY port=");
    Serial.println(kRxPort);
}

void loop()
{
    if (!g_stream.poll())
    {
        delay(2);
        return;
    }

    uint8_t buf[256] = {0};
    const size_t n = g_stream.read(buf, sizeof(buf));
    Serial.print("RX ");
    Serial.print(n);
    Serial.println(" bytes");

    // Echo back to the original sender.
    const IPAddress peer = g_stream.remoteIP();
    const uint16_t peerPort = g_stream.remotePort();
    if (peerPort != 0 && n > 0)
    {
        g_txUdp.beginPacket(peer, peerPort);
        g_txUdp.write(buf, n);
        g_txUdp.endPacket();
    }
}
