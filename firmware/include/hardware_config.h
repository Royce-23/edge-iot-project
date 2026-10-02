#pragma once

// P2 hardware defaults for the ESP32-S3 DevKitC-1. Override any value with a
// PlatformIO build flag when the real board uses different pins or settings.

#ifndef P2_ADXL345_PIN_SCK
#define P2_ADXL345_PIN_SCK 12
#endif

#ifndef P2_ADXL345_PIN_MISO
#define P2_ADXL345_PIN_MISO 13
#endif

#ifndef P2_ADXL345_PIN_MOSI
#define P2_ADXL345_PIN_MOSI 11
#endif

#ifndef P2_ADXL345_PIN_CS
#define P2_ADXL345_PIN_CS 10
#endif

#ifndef P2_DS18B20_PIN
#define P2_DS18B20_PIN 4
#endif

#ifndef P2_ADXL345_SPI_CLOCK_HZ
#define P2_ADXL345_SPI_CLOCK_HZ 5000000UL
#endif

// ADXL345 output-data rate and the software sampling timer intentionally match.
#ifndef P2_SAMPLE_RATE_HZ
#define P2_SAMPLE_RATE_HZ 800U
#endif

// Supported values: 2, 4, 8, or 16 g. Full-resolution mode keeps a scale of
// approximately 3.9 mg/LSB. +/-8 g provides useful headroom for the first rig.
#ifndef P2_ADXL345_RANGE_G
#define P2_ADXL345_RANGE_G 8
#endif

// Software calibration values. Determine these on the final rigid mounting and
// pass them as build flags, for example -DP2_ADXL345_BIAS_X_G=0.0125f.
#ifndef P2_ADXL345_BIAS_X_G
#define P2_ADXL345_BIAS_X_G 0.0f
#endif

#ifndef P2_ADXL345_BIAS_Y_G
#define P2_ADXL345_BIAS_Y_G 0.0f
#endif

#ifndef P2_ADXL345_BIAS_Z_G
#define P2_ADXL345_BIAS_Z_G 0.0f
#endif

#ifndef P2_SAMPLING_BUFFER_CAPACITY
#define P2_SAMPLING_BUFFER_CAPACITY 1024U
#endif

#ifndef P2_SAMPLING_TASK_STACK_SIZE
#define P2_SAMPLING_TASK_STACK_SIZE 4096U
#endif

#ifndef P2_SAMPLING_TASK_PRIORITY
#define P2_SAMPLING_TASK_PRIORITY 3U
#endif

// ESP32-S3 Wi-Fi runs on core 0 in this Arduino/ESP-IDF configuration. Keep
// time-critical SPI acquisition on core 1, above the Arduino loop priority.
#ifndef P2_SAMPLING_TASK_CORE
#define P2_SAMPLING_TASK_CORE 1
#endif

// MQTT/Wi-Fi coordination is deliberately lower priority than sampling and is
// kept with the Wi-Fi side on core 0. ESP-MQTT uses the same priority value.
#ifndef P1_NETWORK_TASK_PRIORITY
#define P1_NETWORK_TASK_PRIORITY 1U
#endif

#ifndef P1_NETWORK_TASK_CORE
#define P1_NETWORK_TASK_CORE 0
#endif

// One active alarm output (active buzzer module or one LED through 330 ohm).
// It is disabled until the actual wiring is confirmed. For the documented
// wiring, GPIO 5 is available and can be enabled with -DP1_ALARM_PIN=5.
#ifndef P1_ALARM_PIN
#define P1_ALARM_PIN -1
#endif

#ifndef P1_ALARM_ACTIVE_HIGH
#define P1_ALARM_ACTIVE_HIGH 1
#endif

// Three optional discrete status LEDs. Each LED needs its own 220-1000 ohm
// series resistor. The current active-low wiring is 3V3 -> resistor -> LED
// anode (long leg), LED cathode (short leg) -> GPIO. Leave all three pins at
// -1 to disable the status display.
#ifndef P1_STATUS_LED_RED_PIN
#define P1_STATUS_LED_RED_PIN -1
#endif

#ifndef P1_STATUS_LED_GREEN_PIN
#define P1_STATUS_LED_GREEN_PIN -1
#endif

#ifndef P1_STATUS_LED_YELLOW_PIN
#define P1_STATUS_LED_YELLOW_PIN -1
#endif

#ifndef P1_STATUS_LED_ACTIVE_LOW
#define P1_STATUS_LED_ACTIVE_LOW 0
#endif

// Fan speed control via a MOSFET module gate (PWM). -1 disables the output.
#ifndef P1_FAN_PWM_PIN
#define P1_FAN_PWM_PIN -1
#endif

#ifndef P1_FAN_PWM_CHANNEL
#define P1_FAN_PWM_CHANNEL 0
#endif

#ifndef P1_FAN_PWM_FREQ_HZ
#define P1_FAN_PWM_FREQ_HZ 1000U
#endif

#ifndef P1_FAN_PWM_RESOLUTION_BITS
#define P1_FAN_PWM_RESOLUTION_BITS 8U
#endif

// Momentary push button that silences an active alarm until it re-triggers.
// Wired to GND; read with INPUT_PULLUP. -1 disables the input.
#ifndef P1_ACK_BUTTON_PIN
#define P1_ACK_BUTTON_PIN -1
#endif

// I2C OLED status display (SSD1306 128x32). Leave both at -1 to disable.
#ifndef P1_OLED_SDA_PIN
#define P1_OLED_SDA_PIN -1
#endif

#ifndef P1_OLED_SCL_PIN
#define P1_OLED_SCL_PIN -1
#endif

#ifndef P1_OLED_I2C_ADDRESS
#define P1_OLED_I2C_ADDRESS 0x3C
#endif

#if P1_ALARM_PIN >= 0
#if P1_ALARM_PIN > 48 || (P1_ALARM_PIN >= 22 && P1_ALARM_PIN <= 34)
#error "P1_ALARM_PIN is not a valid ESP32-S3 DevKitC-1 output"
#endif
#if P1_ALARM_PIN == P2_DS18B20_PIN || P1_ALARM_PIN == P2_ADXL345_PIN_SCK || \
    P1_ALARM_PIN == P2_ADXL345_PIN_MISO ||                              \
    P1_ALARM_PIN == P2_ADXL345_PIN_MOSI || P1_ALARM_PIN == P2_ADXL345_PIN_CS
#error "P1_ALARM_PIN conflicts with an active sensor pin"
#endif
#if P1_ALARM_PIN == 0 || P1_ALARM_PIN == 3 || P1_ALARM_PIN == 19 || \
    P1_ALARM_PIN == 20 || P1_ALARM_PIN == 43 || P1_ALARM_PIN == 44 || \
    P1_ALARM_PIN == 45 || P1_ALARM_PIN == 46 || P1_ALARM_PIN == 48
#error "P1_ALARM_PIN uses a boot/USB/UART/NeoPixel-sensitive pin"
#endif
#endif

#if P1_STATUS_LED_RED_PIN >= 0 || P1_STATUS_LED_GREEN_PIN >= 0 || \
    P1_STATUS_LED_YELLOW_PIN >= 0
#if P1_STATUS_LED_RED_PIN < 0 || P1_STATUS_LED_GREEN_PIN < 0 || \
    P1_STATUS_LED_YELLOW_PIN < 0
#error "Configure all three status LED pins or disable all three"
#endif
#if P1_STATUS_LED_RED_PIN == P1_STATUS_LED_GREEN_PIN || \
    P1_STATUS_LED_RED_PIN == P1_STATUS_LED_YELLOW_PIN ||  \
    P1_STATUS_LED_GREEN_PIN == P1_STATUS_LED_YELLOW_PIN
#error "Status LED pins must be different"
#endif
#if P1_STATUS_LED_RED_PIN == P1_ALARM_PIN || \
    P1_STATUS_LED_GREEN_PIN == P1_ALARM_PIN || \
    P1_STATUS_LED_YELLOW_PIN == P1_ALARM_PIN
#error "Status LED pin conflicts with the buzzer/alarm pin"
#endif
#if P1_STATUS_LED_RED_PIN == P2_DS18B20_PIN || \
    P1_STATUS_LED_GREEN_PIN == P2_DS18B20_PIN || \
    P1_STATUS_LED_YELLOW_PIN == P2_DS18B20_PIN || \
    P1_STATUS_LED_RED_PIN == P2_ADXL345_PIN_SCK || \
    P1_STATUS_LED_GREEN_PIN == P2_ADXL345_PIN_SCK || \
    P1_STATUS_LED_YELLOW_PIN == P2_ADXL345_PIN_SCK || \
    P1_STATUS_LED_RED_PIN == P2_ADXL345_PIN_MISO || \
    P1_STATUS_LED_GREEN_PIN == P2_ADXL345_PIN_MISO || \
    P1_STATUS_LED_YELLOW_PIN == P2_ADXL345_PIN_MISO || \
    P1_STATUS_LED_RED_PIN == P2_ADXL345_PIN_MOSI || \
    P1_STATUS_LED_GREEN_PIN == P2_ADXL345_PIN_MOSI || \
    P1_STATUS_LED_YELLOW_PIN == P2_ADXL345_PIN_MOSI || \
    P1_STATUS_LED_RED_PIN == P2_ADXL345_PIN_CS || \
    P1_STATUS_LED_GREEN_PIN == P2_ADXL345_PIN_CS || \
    P1_STATUS_LED_YELLOW_PIN == P2_ADXL345_PIN_CS
#error "Status LED pin conflicts with an active sensor pin"
#endif
#if P1_STATUS_LED_RED_PIN > 48 || P1_STATUS_LED_GREEN_PIN > 48 || \
    P1_STATUS_LED_YELLOW_PIN > 48 || \
    (P1_STATUS_LED_RED_PIN >= 22 && P1_STATUS_LED_RED_PIN <= 34) || \
    (P1_STATUS_LED_GREEN_PIN >= 22 && P1_STATUS_LED_GREEN_PIN <= 34) || \
    (P1_STATUS_LED_YELLOW_PIN >= 22 && P1_STATUS_LED_YELLOW_PIN <= 34)
#error "Status LED pin is not a valid ESP32-S3 DevKitC-1 output"
#endif
#endif

#if P1_FAN_PWM_PIN >= 0
#if P1_FAN_PWM_PIN > 48 || (P1_FAN_PWM_PIN >= 22 && P1_FAN_PWM_PIN <= 34)
#error "P1_FAN_PWM_PIN is not a valid ESP32-S3 DevKitC-1 output"
#endif
#if P1_FAN_PWM_PIN == 0 || P1_FAN_PWM_PIN == 3 || P1_FAN_PWM_PIN == 19 || \
    P1_FAN_PWM_PIN == 20 || P1_FAN_PWM_PIN == 43 || P1_FAN_PWM_PIN == 44 || \
    P1_FAN_PWM_PIN == 45 || P1_FAN_PWM_PIN == 46 || P1_FAN_PWM_PIN == 48
#error "P1_FAN_PWM_PIN uses a boot/USB/UART/NeoPixel-sensitive pin"
#endif
#if P1_FAN_PWM_PIN == P2_DS18B20_PIN || P1_FAN_PWM_PIN == P2_ADXL345_PIN_SCK || \
    P1_FAN_PWM_PIN == P2_ADXL345_PIN_MISO ||                                \
    P1_FAN_PWM_PIN == P2_ADXL345_PIN_MOSI ||                                \
    P1_FAN_PWM_PIN == P2_ADXL345_PIN_CS || P1_FAN_PWM_PIN == P1_ALARM_PIN || \
    P1_FAN_PWM_PIN == P1_STATUS_LED_RED_PIN ||                              \
    P1_FAN_PWM_PIN == P1_STATUS_LED_GREEN_PIN ||                           \
    P1_FAN_PWM_PIN == P1_STATUS_LED_YELLOW_PIN
#error "P1_FAN_PWM_PIN conflicts with an active pin"
#endif
#endif

#if P1_ACK_BUTTON_PIN >= 0
#if P1_ACK_BUTTON_PIN > 48 || (P1_ACK_BUTTON_PIN >= 22 && P1_ACK_BUTTON_PIN <= 34)
#error "P1_ACK_BUTTON_PIN is not a valid ESP32-S3 DevKitC-1 input"
#endif
#if P1_ACK_BUTTON_PIN == 0 || P1_ACK_BUTTON_PIN == 3 || \
    P1_ACK_BUTTON_PIN == 19 || P1_ACK_BUTTON_PIN == 20 || \
    P1_ACK_BUTTON_PIN == 43 || P1_ACK_BUTTON_PIN == 44 || \
    P1_ACK_BUTTON_PIN == 45 || P1_ACK_BUTTON_PIN == 46 || \
    P1_ACK_BUTTON_PIN == 48
#error "P1_ACK_BUTTON_PIN uses a boot/USB/UART/NeoPixel-sensitive pin"
#endif
#if P1_ACK_BUTTON_PIN == P2_DS18B20_PIN ||                                  \
    P1_ACK_BUTTON_PIN == P2_ADXL345_PIN_SCK ||                             \
    P1_ACK_BUTTON_PIN == P2_ADXL345_PIN_MISO ||                            \
    P1_ACK_BUTTON_PIN == P2_ADXL345_PIN_MOSI ||                            \
    P1_ACK_BUTTON_PIN == P2_ADXL345_PIN_CS ||                              \
    P1_ACK_BUTTON_PIN == P1_ALARM_PIN ||                                   \
    P1_ACK_BUTTON_PIN == P1_STATUS_LED_RED_PIN ||                          \
    P1_ACK_BUTTON_PIN == P1_STATUS_LED_GREEN_PIN ||                        \
    P1_ACK_BUTTON_PIN == P1_STATUS_LED_YELLOW_PIN ||                       \
    P1_ACK_BUTTON_PIN == P1_FAN_PWM_PIN
#error "P1_ACK_BUTTON_PIN conflicts with an active pin"
#endif
#endif

#if P1_OLED_SDA_PIN >= 0 || P1_OLED_SCL_PIN >= 0
#if P1_OLED_SDA_PIN < 0 || P1_OLED_SCL_PIN < 0
#error "Configure both P1_OLED_SDA_PIN and P1_OLED_SCL_PIN or disable both"
#endif
#if P1_OLED_SDA_PIN == P1_OLED_SCL_PIN
#error "P1_OLED_SDA_PIN and P1_OLED_SCL_PIN must be different"
#endif
#if P1_OLED_SDA_PIN > 48 || P1_OLED_SCL_PIN > 48 || \
    (P1_OLED_SDA_PIN >= 22 && P1_OLED_SDA_PIN <= 34) || \
    (P1_OLED_SCL_PIN >= 22 && P1_OLED_SCL_PIN <= 34)
#error "P1_OLED_SDA_PIN/P1_OLED_SCL_PIN is not a valid ESP32-S3 DevKitC-1 pin"
#endif
#if P1_OLED_SDA_PIN == 0 || P1_OLED_SDA_PIN == 3 || P1_OLED_SDA_PIN == 19 || \
    P1_OLED_SDA_PIN == 20 || P1_OLED_SDA_PIN == 43 ||                     \
    P1_OLED_SDA_PIN == 44 || P1_OLED_SDA_PIN == 45 ||                     \
    P1_OLED_SDA_PIN == 46 || P1_OLED_SDA_PIN == 48 ||                     \
    P1_OLED_SCL_PIN == 0 || P1_OLED_SCL_PIN == 3 || P1_OLED_SCL_PIN == 19 || \
    P1_OLED_SCL_PIN == 20 || P1_OLED_SCL_PIN == 43 ||                     \
    P1_OLED_SCL_PIN == 44 || P1_OLED_SCL_PIN == 45 ||                     \
    P1_OLED_SCL_PIN == 46 || P1_OLED_SCL_PIN == 48
#error "P1_OLED_SDA_PIN/P1_OLED_SCL_PIN uses a boot/USB/UART/NeoPixel-sensitive pin"
#endif
#if P1_OLED_SDA_PIN == P2_DS18B20_PIN || P1_OLED_SDA_PIN == P2_ADXL345_PIN_SCK || \
    P1_OLED_SDA_PIN == P2_ADXL345_PIN_MISO || P1_OLED_SDA_PIN == P2_ADXL345_PIN_MOSI || \
    P1_OLED_SDA_PIN == P2_ADXL345_PIN_CS || P1_OLED_SDA_PIN == P1_ALARM_PIN || \
    P1_OLED_SDA_PIN == P1_STATUS_LED_RED_PIN || P1_OLED_SDA_PIN == P1_STATUS_LED_GREEN_PIN || \
    P1_OLED_SDA_PIN == P1_STATUS_LED_YELLOW_PIN || P1_OLED_SDA_PIN == P1_FAN_PWM_PIN || \
    P1_OLED_SDA_PIN == P1_ACK_BUTTON_PIN || \
    P1_OLED_SCL_PIN == P2_DS18B20_PIN || P1_OLED_SCL_PIN == P2_ADXL345_PIN_SCK || \
    P1_OLED_SCL_PIN == P2_ADXL345_PIN_MISO || P1_OLED_SCL_PIN == P2_ADXL345_PIN_MOSI || \
    P1_OLED_SCL_PIN == P2_ADXL345_PIN_CS || P1_OLED_SCL_PIN == P1_ALARM_PIN || \
    P1_OLED_SCL_PIN == P1_STATUS_LED_RED_PIN || P1_OLED_SCL_PIN == P1_STATUS_LED_GREEN_PIN || \
    P1_OLED_SCL_PIN == P1_STATUS_LED_YELLOW_PIN || P1_OLED_SCL_PIN == P1_FAN_PWM_PIN || \
    P1_OLED_SCL_PIN == P1_ACK_BUTTON_PIN
#error "P1_OLED_SDA_PIN/P1_OLED_SCL_PIN conflicts with an active pin"
#endif
#endif
