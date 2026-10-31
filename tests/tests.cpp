#include "settings.h"
#include "dsp.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <windows.h>

namespace {
struct ConfigCleanup {
    ~ConfigCleanup() {
        const std::wstring path = settings::GetSettingsPath();
        if (!path.empty()) {
            DeleteFileW(path.c_str());
        }
    }
};

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
            return false; \
        } \
    } while (false)

bool TestIniRoundTrip() {
    ConfigCleanup cleanup;
    CHECK(settings::LoadRememberedInputId().empty());
    settings::SaveRememberedInputId(L"device-test-id");
    CHECK(settings::LoadRememberedInputId() == L"device-test-id");
    settings::SaveRememberedInputId(L"");
    CHECK(settings::LoadRememberedInputId() == L"device-test-id");
    return true;
}

bool TestHighRateResamplingAcrossPackets() {
    AudioRingBuffer ring(4096, 1);
    StreamConverter converter;
    converter.Reset(192000, 1, 48000, 1);
    const std::vector<float> packet(480, 0.5f);
    converter.Process(packet.data(), packet.size(), 0.995, ring);
    const size_t first = ring.Size();
    CHECK(first >= 100 && first <= 130);
    converter.Process(packet.data(), packet.size(), 0.995, ring);
    const size_t total = ring.Size();
    CHECK(total >= 200 && total <= 260);
    std::vector<float> output(total);
    CHECK(ring.Read(output.data(), output.size()) == total);
    for (float sample : output) {
        CHECK(std::abs(sample - 0.5f) < 0.00001f);
    }
    return true;
}

bool TestRingBufferDropsOldestAndPadsSilence() {
    AudioRingBuffer ring(4, 1);
    const float input[] = {0, 1, 2, 3, 4, 5};
    ring.Write(input, 6);
    CHECK(ring.Size() == 4);
    float output[6]{};
    CHECK(ring.Read(output, 6) == 4);
    CHECK(output[0] == 2 && output[1] == 3 && output[2] == 4 && output[3] == 5);
    CHECK(output[4] == 0 && output[5] == 0);
    return true;
}

bool TestSoftClipPreservesNormalAndBoundsPeaks() {
    const PcmFormat format{SampleKind::Float32, 48000, 1};
    const float input[] = {-2, -1.2f, -1, -0.8f, -0.5f, 0, 0.5f, 0.8f, 1, 1.2f, 2};
    BYTE output[sizeof(input)]{};
    ConvertFromFloat(input, 11, format, output);
    const auto* samples = reinterpret_cast<const float*>(output);
    CHECK(samples[0] == -1 && samples[1] == -1);
    CHECK(std::abs(samples[2] + 0.95f) < 0.00001f);
    CHECK(samples[3] == -0.8f && samples[4] == -0.5f && samples[5] == 0);
    CHECK(samples[6] == 0.5f && samples[7] == 0.8f);
    CHECK(std::abs(samples[8] - 0.95f) < 0.00001f);
    CHECK(samples[9] == 1 && samples[10] == 1);
    return true;
}

bool TestNoiseGateClosesOpensAndBypassesSmoothly() {
    NoiseGate gate;
    gate.Reset(48000, 1);
    std::vector<float> quiet(24000, 0.001f);
    gate.Process(quiet.data(), quiet.size(), true);
    CHECK(quiet.back() == 0);
    std::vector<float> voice(4800, 0.2f);
    gate.Process(voice.data(), voice.size(), true);
    CHECK(voice.back() > 0.15f);
    std::vector<float> bypass(4800, 0.2f);
    gate.Process(bypass.data(), bypass.size(), false);
    CHECK(bypass.back() > 0.199f);
    return true;
}

using TestFunction = bool (*)();
struct NamedTest {
    const char* name;
    TestFunction run;
};
}

int main() {
    const NamedTest tests[] = {
        {"INI round trip", TestIniRoundTrip},
        {"192 to 48 kHz across packets", TestHighRateResamplingAcrossPackets},
        {"ring buffer overflow and silence", TestRingBufferDropsOldestAndPadsSilence},
        {"soft clip boundaries", TestSoftClipPreservesNormalAndBoundsPeaks},
        {"noise gate transitions", TestNoiseGateClosesOpensAndBypassesSmoothly},
    };
    int failures = 0;
    for (const auto& test : tests) {
        if (test.run()) {
            std::printf("[PASS] %s\n", test.name);
        } else {
            std::printf("[FAIL] %s\n", test.name);
            ++failures;
        }
    }
    std::printf("%zu tests, %d failed\n", std::size(tests), failures);
    return failures == 0 ? 0 : 1;
}
