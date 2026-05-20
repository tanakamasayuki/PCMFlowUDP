// Smoke test sketch — verifies the PCMFlowUDP library compiles against
// the chosen profile and that the test harness wiring works.
// Once the transport implementation lands, this stays as a build-only
// check.

#include <PCMFlowUDP.h>

void setup()
{
    Serial.begin(115200);
    delay(2000);
    Serial.print("PCMFlowUDP ");
    Serial.println(PCMFLOWUDP_VERSION_STR);
    Serial.println("SMOKE ready");
}

void loop()
{
    delay(1);
}
