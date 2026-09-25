#include "display.h"

#include "hardware_config.h"

#if P1_OLED_SDA_PIN >= 0 && P1_OLED_SCL_PIN >= 0
#define P1_OLED_ENABLED 1
#else
#define P1_OLED_ENABLED 0
#endif

#include <Arduino.h>

#if P1_OLED_ENABLED
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#endif

#include "device_state.h"

namespace {

#if P1_OLED_ENABLED
constexpr uint8_t kScreenWidth = 128;
constexpr uint8_t kScreenHeight = 32;
Adafruit_SSD1306 display(kScreenWidth, kScreenHeight, &Wire, -1);
#endif

bool displayReady = false;

}  // namespace

void initDisplay() {
#if P1_OLED_ENABLED
    Wire.begin(P1_OLED_SDA_PIN, P1_OLED_SCL_PIN);
    // Without this, a disconnected/unresponsive OLED hangs the I2C driver
    // forever (no ACK, no clock-stretch timeout), freezing the whole device
    // (sampling, MQTT, button) since everything runs on one core-1 loop.
    Wire.setTimeOut(50);

    Serial.println("[OLED] Scanning I2C bus...");
    int foundCount = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[OLED] Found device at 0x%02X\n", addr);
            foundCount++;
        }
    }
    if (foundCount == 0) {
        Serial.println(
            "[OLED] No I2C device responded on the bus at all (check "
            "wiring/power/GND)");
    }

    if (!display.begin(SSD1306_SWITCHCAPVCC, P1_OLED_I2C_ADDRESS)) {
        Serial.println(
            "[OLED] SSD1306 init failed; check wiring/I2C address");
        return;
    }
    displayReady = true;
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Starting...");
    display.display();
    Serial.printf("[OLED] initialized sda=%d scl=%d addr=0x%02X\n",
                  P1_OLED_SDA_PIN, P1_OLED_SCL_PIN, P1_OLED_I2C_ADDRESS);
#else
    Serial.println(
        "[OLED] GPIO disabled; configure P1_OLED_SDA_PIN/SCL_PIN after "
        "wiring");
#endif
}

void updateDisplay(HealthState health, float rms, bool hasTemperature,
                   float temperatureC, bool fanOn, uint8_t fanSpeedPercent,
                   bool networkOnline) {
#if P1_OLED_ENABLED
    if (!displayReady) {
        return;
    }
    display.clearDisplay();

    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println(healthName(health));

    display.setTextSize(1);
    display.setCursor(0, 18);
    display.printf("RMS %.3fg", rms);
    if (hasTemperature) {
        display.printf(" %.1fC", temperatureC);
    }

    display.setCursor(0, 26);
    display.printf("Fan %s %u%%  Net %s", fanOn ? "ON" : "OFF",
                   fanSpeedPercent, networkOnline ? "OK" : "--");

    display.display();
#else
    (void)health;
    (void)rms;
    (void)hasTemperature;
    (void)temperatureC;
    (void)fanOn;
    (void)fanSpeedPercent;
    (void)networkOnline;
#endif
}
