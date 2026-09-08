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

