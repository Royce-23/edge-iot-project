#include <Arduino.h>
// P1 owns setup()/loop(). Call P2/P3 modules after implementation.
void setup() {
    Serial.begin(115200);
    Serial.println("IoT starter: modules are not implemented yet.");
}
void loop() {
    delay(1000);
}
