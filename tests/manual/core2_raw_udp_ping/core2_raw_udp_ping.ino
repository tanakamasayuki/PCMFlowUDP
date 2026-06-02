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
static constexpr uint16_t kRxPort = 49220;

WiFiUDP g_rxUdp;
WiFiUDP g_txUdp;
RawUdpStream g_stream(g_rxUdp);

static uint32_t crc32Update(uint32_t crc, uint8_t byte)
{
    crc ^= byte;
    for (uint8_t i = 0; i < 8; ++i)
    {
        const uint32_t mask = 0u - (crc & 1u);
        crc = (crc >> 1) ^ (0xEDB88320u & mask);
    }
    return crc;
}

static uint32_t crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i)
        crc = crc32Update(crc, data[i]);
    return ~crc;
}

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
    {
        delay(250);
    }

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
    M5.Display.println("RAW UDP ping");
    M5.Display.println();
    M5.Display.print("IP: ");
    M5.Display.println(ip);
    M5.Display.print("Port: ");
    M5.Display.println(kRxPort);
    M5.Display.println();
    M5.Display.println("Waiting...");
}

static void drawReceived(size_t len, uint32_t crc, const IPAddress &peer, uint16_t port)
{
    M5.Display.clear();
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(2);
    M5.Display.println("RAW UDP ping");
    M5.Display.println();
    M5.Display.print("RX: ");
    M5.Display.println(len);
    M5.Display.print("CRC: ");
    M5.Display.println(crc, HEX);
    M5.Display.print("Peer: ");
    M5.Display.print(peer);
    M5.Display.print(":");
    M5.Display.println(port);
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

    if (g_txUdp.begin(0) != 1)
    {
        Serial.println("FAIL tx-begin");
        M5.Display.println("TX begin failed");
        return;
    }

    if (!g_stream.begin(kRxPort))
    {
        Serial.println("FAIL stream-begin");
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

    if (!g_stream.poll())
    {
        delay(2);
        return;
    }

    uint8_t buf[256] = {0};
    const size_t n = g_stream.read(buf, sizeof(buf));
    const uint32_t crc = crc32(buf, n);
    const IPAddress peer = g_stream.remoteIP();
    const uint16_t peerPort = g_stream.remotePort();

    Serial.print("RAW-RX len=");
    Serial.print(n);
    Serial.print(" crc=");
    Serial.print(crc, HEX);
    Serial.print(" peer=");
    Serial.print(peer);
    Serial.print(":");
    Serial.println(peerPort);

    drawReceived(n, crc, peer, peerPort);

    if (peerPort != 0 && n > 0)
    {
        g_txUdp.beginPacket(peer, peerPort);
        g_txUdp.write(buf, n);
        g_txUdp.endPacket();
        Serial.println("RAW-ACK sent");
    }
}
