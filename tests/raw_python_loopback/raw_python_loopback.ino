// Python ↔ DUT bidirectional RAW UDP loopback.
//
// On startup the DUT prints `DUT-READY ip=<X> port=<N>` so the Python
// test can discover both the address and the local port. On ESP32 it
// joins the WiFi network defined at build time via WIFI_SSID /
// WIFI_PASSWORD (see build_config.toml + .env). On the lang-ship:host
// profile it falls back to 127.0.0.1.
//
// The DUT polls for one datagram, echoes it back to the sender, and
// prints `RX <N> bytes` so Python can observe the receive over Serial.

#include <WiFiUdp.h>
#include <PCMFlowUDP.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#endif

static constexpr uint16_t kRxPort = 49200;

WiFiUDP g_rxUdp;
WiFiUDP g_txUdp;
RawUdpStream g_stream(g_rxUdp);

static IPAddress connectAndReportIp()
{
#if defined(ARDUINO_ARCH_ESP32)
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    const unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 60000)
    {
        delay(250);
    }
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WIFI_ERROR connect_failed");
        while (true)
            delay(1000);
    }
    return WiFi.localIP();
#else
    return IPAddress(127, 0, 0, 1);
#endif
}

void setup()
{
    Serial.begin(115200);
    delay(5000); // ESP32 Serial-USB bridge can drop early output otherwise

    const IPAddress localIp = connectAndReportIp();

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

    Serial.print("DUT-READY ip=");
    Serial.print(localIp);
    Serial.print(" port=");
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
