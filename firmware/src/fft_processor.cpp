// Owner: P3
#include "fft_processor.h"

#include <cmath>
#include <complex>
#include <utility>

#include "features.h"

namespace {

constexpr double PI = 3.14159265358979323846;
std::complex<float> spectrum[P3_MAX_SAMPLE_COUNT];

bool isPowerOfTwo(std::size_t value) {
    return value >= 4 && (value & (value - 1U)) == 0;
}

}  // namespace

bool processFFT(const float* samples, std::size_t count, float sampleRateHz,
                float meanToRemove, float bandLowHz, float bandHighHz,
                SpectralFeatures& output) {
    output = {NAN, NAN};
    if (samples == nullptr || !isPowerOfTwo(count) ||
        count > P3_MAX_SAMPLE_COUNT || !std::isfinite(sampleRateHz) ||
        sampleRateHz <= 0.0f || !std::isfinite(meanToRemove) ||
        !std::isfinite(bandLowHz) || !std::isfinite(bandHighHz) ||
        bandLowHz < 0.0f || bandHighHz <= bandLowHz ||
        bandHighHz > sampleRateHz / 2.0f) {
        return false;
    }

    double windowSquareSum = 0.0;
    for (std::size_t index = 0; index < count; ++index) {
        if (!std::isfinite(samples[index])) {
            return false;
        }
        const float window = static_cast<float>(
            0.5 - 0.5 * std::cos(2.0 * PI * index / (count - 1U)));
        spectrum[index] = {(samples[index] - meanToRemove) * window, 0.0f};
        windowSquareSum += static_cast<double>(window) * window;
    }

    for (std::size_t index = 1, reversed = 0; index < count; ++index) {
        std::size_t bit = count >> 1U;
        while ((reversed & bit) != 0U) {
            reversed ^= bit;
            bit >>= 1U;
        }
        reversed ^= bit;
        if (index < reversed) {
            std::swap(spectrum[index], spectrum[reversed]);
        }
    }

    for (std::size_t length = 2; length <= count; length <<= 1U) {
        const double angle = -2.0 * PI / length;
        const std::complex<float> step(
            static_cast<float>(std::cos(angle)),
            static_cast<float>(std::sin(angle)));
        for (std::size_t start = 0; start < count; start += length) {
            std::complex<float> phase(1.0f, 0.0f);
            for (std::size_t offset = 0; offset < length / 2U; ++offset) {
                const auto even = spectrum[start + offset];
                const auto odd = spectrum[start + offset + length / 2U] * phase;
                spectrum[start + offset] = even + odd;
                spectrum[start + offset + length / 2U] = even - odd;
                phase *= step;
            }
        }
    }

    const double denominator = count * windowSquareSum;
    double bandEnergy = 0.0;
    double strongestPower = 0.0;
    float dominantFrequency = 0.0f;
    for (std::size_t bin = 0; bin <= count / 2U; ++bin) {
        const double real = spectrum[bin].real();
        const double imaginary = spectrum[bin].imag();
        double power = (real * real + imaginary * imaginary) / denominator;
        if (bin > 0 && bin < count / 2U) {
            power *= 2.0;
        }
        const float frequency =
            static_cast<float>(bin) * sampleRateHz / static_cast<float>(count);
        if (frequency >= bandLowHz && frequency <= bandHighHz) {
            bandEnergy += power;
        }
        if (bin > 0 && power > strongestPower) {
            strongestPower = power;
            dominantFrequency = frequency;
        }
    }

    output = {dominantFrequency, static_cast<float>(bandEnergy)};
    return std::isfinite(output.dominantFrequency) &&
           std::isfinite(output.bandEnergy) && output.bandEnergy >= 0.0f;
}