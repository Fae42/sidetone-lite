#pragma once

#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <ksmedia.h>
#include <cstddef>
#include <cstdint>
#include <vector>

enum class SampleKind {
    Float32,
    Int16,
    Int24,
    Int32,
};

struct PcmFormat {
    SampleKind kind;
    uint32_t sampleRate;
    uint16_t channels;
};

bool DescribePcmFormat(const WAVEFORMATEX& format, PcmFormat& result);
void ConvertToFloat(const BYTE* input, size_t frames, const PcmFormat& format, float* output);
void ConvertFromFloat(const float* input, size_t frames, const PcmFormat& format, BYTE* output);

class AudioRingBuffer {
public:
    AudioRingBuffer() = default;
    AudioRingBuffer(size_t capacityFrames, uint16_t channels);

    void Reset(size_t capacityFrames, uint16_t channels);
    void Write(const float* input, size_t frames);
    size_t Read(float* output, size_t frames);
    size_t Size() const;

private:
    void Drop(size_t frames);

    std::vector<float> data_;
    size_t capacityFrames_ = 0;
    size_t readFrame_ = 0;
    size_t writeFrame_ = 0;
    size_t countFrames_ = 0;
    uint16_t channels_ = 0;
};

class StreamConverter {
public:
    void Reset(uint32_t inputRate, uint16_t inputChannels, uint32_t outputRate, uint16_t outputChannels);
    void Process(const float* input, size_t inputFrames, double speed, AudioRingBuffer& output);

private:
    std::vector<float> pending_;
    std::vector<float> converted_;
    double position_ = 0;
    uint32_t inputRate_ = 0;
    uint32_t outputRate_ = 0;
    uint16_t inputChannels_ = 0;
    uint16_t outputChannels_ = 0;
};

class NoiseGate {
public:
    void Reset(uint32_t sampleRate, uint16_t channels);
    void Process(float* samples, size_t frames, bool enabled);

private:
    uint16_t channels_ = 0;
    float threshold_ = 0;
    float attackStep_ = 0;
    float releaseStep_ = 0;
    float gain_ = 1;
    size_t holdFrames_ = 0;
    size_t holdRemaining_ = 0;
};
