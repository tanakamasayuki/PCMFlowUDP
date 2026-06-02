#include <M5Unified.h>
#include <WiFi.h>
#include <PCMFlowUDP.h>

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

static constexpr unsigned long kWifiTimeoutMs = 60000;

static bool g_reportedA = false;
static bool g_reportedB = false;
static bool g_reportedC = false;
static unsigned long g_lastStatusMs = 0;

static void drawStatus(const IPAddress &ip)
{
    M5.Display.clear();
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(2);
    M5.Display.println("PCMFlowUDP Core2");
    M5.Display.println("smoke");
    M5.Display.println();
    M5.Display.print("IP: ");
    M5.Display.println(ip);
    M5.Display.print("RSSI: ");
    M5.Display.println(WiFi.RSSI());
    M5.Display.println();
    M5.Display.println("Press A/B/C");
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

void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);

    Serial.begin(115200);
    delay(5000);

    M5.Display.setTextSize(2);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Connecting WiFi...");

    IPAddress ip;
    if (!connectWifi(ip))
    {
        M5.Display.println("WiFi failed");
        return;
    }

    drawStatus(ip);

    Serial.print("CORE2-READY ip=");
    Serial.print(ip);
    Serial.print(" rssi=");
    Serial.println(WiFi.RSSI());
}

void loop()
{
    M5.update();

    if (M5.BtnA.wasPressed() && !g_reportedA)
    {
        g_reportedA = true;
        Serial.println("BUTTON A");
    }
    if (M5.BtnB.wasPressed() && !g_reportedB)
    {
        g_reportedB = true;
        Serial.println("BUTTON B");
    }
    if (M5.BtnC.wasPressed() && !g_reportedC)
    {
        g_reportedC = true;
        Serial.println("BUTTON C");
    }

    if (millis() - g_lastStatusMs >= 5000)
    {
        g_lastStatusMs = millis();
        Serial.print("CORE2-STATUS wifi=");
        Serial.print(static_cast<int>(WiFi.status()));
        Serial.print(" rssi=");
        Serial.print(WiFi.RSSI());
        Serial.print(" heap=");
        Serial.println(ESP.getFreeHeap());
    }

    if (g_reportedA && g_reportedB && g_reportedC)
    {
        Serial.println("CORE2-SMOKE done");
        while (true)
        {
            delay(1000);
        }
    }

    delay(10);
}
