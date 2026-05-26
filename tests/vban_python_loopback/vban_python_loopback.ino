// Python → DUT VBAN audio packet test.
//
// Reports `DUT-READY ip=<X> port=<N>` so Python can target the DUT
// regardless of profile: 127.0.0.1 on host, WiFi IP on ESP32 (joined
// using build-time WIFI_SSID / WIFI_PASSWORD defines).

#include <WiFiUdp.h>
#include <PCMFlowUDP.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#endif

static constexpr uint16_t kPort = 49210;

WiFiUDP g_rxUdp;
VbanReceiver g_rx(g_rxUdp);

static IPAddress connectAndReportIp()
{
#if defined(ARDUINO_ARCH_ESP32)
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    const unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 60000)
        delay(250);
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WIFI_ERROR connect_failed");
        while (true) delay(1000);
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

    if (!g_rx.begin(kPort))
    {
        Serial.println("FAIL rx-begin");
        while (true) delay(1000);
    }
    Serial.print("DUT-READY ip=");
    Serial.print(localIp);
    Serial.print(" port=");
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
