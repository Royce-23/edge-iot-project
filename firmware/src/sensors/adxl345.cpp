// Owner: P2
#include "sensors/adxl345.h"

#include <Arduino.h>
#include <SPI.h>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "hardware_config.h"

namespace {

constexpr uint8_t kRegisterDeviceId = 0x00;
constexpr uint8_t kRegisterBandwidthRate = 0x2C;
constexpr uint8_t kRegisterPowerControl = 0x2D;
constexpr uint8_t kRegisterDataFormat = 0x31;
constexpr uint8_t kRegisterDataX0 = 0x32;

constexpr uint8_t kExpectedDeviceId = 0xE5;
constexpr uint8_t kReadBit = 0x80;
constexpr uint8_t kMultiByteBit = 0x40;
constexpr uint8_t kMeasureBit = 0x08;
constexpr uint8_t kFullResolutionBit = 0x08;
constexpr float kFullResolutionGPerLsb = 0.00390625f;

bool gInitialized = false;

uint8_t outputDataRateCode() {
    switch (P2_SAMPLE_RATE_HZ) {
        case 100:
            return 0x0A;
        case 200:
            return 0x0B;
        case 400:
            return 0x0C;
        case 800:
            return 0x0D;
        case 1600:
            return 0x0E;
        case 3200:
            return 0x0F;
        default:
            return 0;
    }
}

uint8_t rangeCode() {
    switch (P2_ADXL345_RANGE_G) {
        case 2:
            return 0;
        case 4:
            return 1;
        case 8:
            return 2;
        case 16:
            return 3;
        default:
            return 0xFF;
    }
}

void selectSensor() {
    digitalWrite(P2_ADXL345_PIN_CS, LOW);
}

void deselectSensor() {
    digitalWrite(P2_ADXL345_PIN_CS, HIGH);
}

uint8_t readRegister(uint8_t address) {
    SPI.beginTransaction(SPISettings(P2_ADXL345_SPI_CLOCK_HZ, MSBFIRST, SPI_MODE3));
    selectSensor();
    SPI.transfer(address | kReadBit);
    const uint8_t value = SPI.transfer(0x00);
    deselectSensor();
    SPI.endTransaction();
    return value;
}

void writeRegister(uint8_t address, uint8_t value) {
    SPI.beginTransaction(SPISettings(P2_ADXL345_SPI_CLOCK_HZ, MSBFIRST, SPI_MODE3));
    selectSensor();
    SPI.transfer(address);
    SPI.transfer(value);
    deselectSensor();
    SPI.endTransaction();
}

bool readRegisters(uint8_t startAddress, uint8_t* output, std::size_t count) {
    if (output == nullptr || count == 0) {
        return false;
    }

    SPI.beginTransaction(SPISettings(P2_ADXL345_SPI_CLOCK_HZ, MSBFIRST, SPI_MODE3));
    selectSensor();
    SPI.transfer(startAddress | kReadBit | kMultiByteBit);
    for (std::size_t index = 0; index < count; ++index) {
        output[index] = SPI.transfer(0x00);
    }
    deselectSensor();
    SPI.endTransaction();
    return true;
}

int16_t signedValue(uint8_t low, uint8_t high) {
    return static_cast<int16_t>(static_cast<uint16_t>(low) |
                                (static_cast<uint16_t>(high) << 8));
}

}  // namespace

namespace sensors {

bool initAdxl345() {
    const uint8_t rate = outputDataRateCode();
    const uint8_t range = rangeCode();
    if (rate == 0 || range == 0xFF) {
        return false;
    }

    pinMode(P2_ADXL345_PIN_CS, OUTPUT);
    deselectSensor();
    SPI.begin(P2_ADXL345_PIN_SCK, P2_ADXL345_PIN_MISO, P2_ADXL345_PIN_MOSI,
              P2_ADXL345_PIN_CS);
    delay(10);

    if (readRegister(kRegisterDeviceId) != kExpectedDeviceId) {
        return false;
    }

    // Configure in standby, then enter measurement mode after all settings are
    // stable. The ADXL345 bandwidth is half its output-data rate.
    writeRegister(kRegisterPowerControl, 0x00);
    writeRegister(kRegisterBandwidthRate, rate);
    writeRegister(kRegisterDataFormat, kFullResolutionBit | range);
    writeRegister(kRegisterPowerControl, kMeasureBit);
    delay(10);

    const bool validRate = (readRegister(kRegisterBandwidthRate) & 0x0F) == rate;
    const bool validFormat =
        (readRegister(kRegisterDataFormat) & 0x0F) == (kFullResolutionBit | range);
    const bool measuring =
        (readRegister(kRegisterPowerControl) & kMeasureBit) != 0;
    gInitialized = validRate && validFormat && measuring;
    return gInitialized;
}

bool readAdxl345(AccelerationSample& sample) {
    if (!gInitialized) {
        return false;
    }

    uint8_t bytes[6] = {};
    if (!readRegisters(kRegisterDataX0, bytes, sizeof(bytes))) {
        return false;
    }

    // SPI itself has no acknowledgement. A disconnected/pulled MISO line
    // commonly returns six 0x00 or six 0xFF bytes and would otherwise look
    // like a finite, healthy acceleration sample. Gravity makes an all-zero
    // XYZ vector invalid for this fixed fan installation.
    bool allZero = true;
    bool allOnes = true;
    for (uint8_t value : bytes) {
        allZero = allZero && value == 0x00;
        allOnes = allOnes && value == 0xFF;
    }
    if (allZero || allOnes) {
        return false;
    }

    sample.xG = static_cast<float>(signedValue(bytes[0], bytes[1])) *
                    kFullResolutionGPerLsb -
                P2_ADXL345_BIAS_X_G;
    sample.yG = static_cast<float>(signedValue(bytes[2], bytes[3])) *
                    kFullResolutionGPerLsb -
                P2_ADXL345_BIAS_Y_G;
    sample.zG = static_cast<float>(signedValue(bytes[4], bytes[5])) *
                    kFullResolutionGPerLsb -
                P2_ADXL345_BIAS_Z_G;

    return std::isfinite(sample.xG) && std::isfinite(sample.yG) &&
           std::isfinite(sample.zG);
}

}  // namespace sensors
