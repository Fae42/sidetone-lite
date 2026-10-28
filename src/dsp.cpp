#include "dsp.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
template <typename T>
T ReadUnaligned(const BYTE* input) {
    T value;
    std::memcpy(&value, input, sizeof(value));
    return value;
}
}

bool DescribePcmFormat(const WAVEFORMATEX& format, PcmFormat& result) {
    WORD tag = format.wFormatTag;
    WORD bits = format.wBitsPerSample;
    if (tag == WAVE_FORMAT_EXTENSIBLE) {
        if (format.cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
            return false;
        }
        const auto& extended = reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(format);
        if (extended.SubFormat == KSDATAFORMAT_SUBTYPE_PCM) {
            tag = WAVE_FORMAT_PCM;
        } else if (extended.SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
            tag = WAVE_FORMAT_IEEE_FLOAT;
        } else {
            return false;
        }
    }

    result.sampleRate = format.nSamplesPerSec;
    result.channels = format.nChannels;
    if (result.sampleRate == 0 || result.channels == 0) {
        return false;
    }
    if (tag == WAVE_FORMAT_IEEE_FLOAT && bits == 32) {
        result.kind = SampleKind::Float32;
        return true;
    }
    if (tag == WAVE_FORMAT_PCM) {
        if (bits == 16) {
            result.kind = SampleKind::Int16;
            return true;
        }
        if (bits == 24) {
            result.kind = SampleKind::Int24;
            return true;
        }
        if (bits == 32) {
            result.kind = SampleKind::Int32;
            return true;
        }
    }
    return false;
}

void ConvertToFloat(const BYTE* input, size_t frames, const PcmFormat& format, float* output) {
    const size_t samples = frames * format.channels;
    for (size_t i = 0; i < samples; ++i) {
        switch (format.kind) {
        case SampleKind::Float32:
            output[i] = ReadUnaligned<float>(input + i * 4);
            break;
        case SampleKind::Int16:
            output[i] = static_cast<float>(ReadUnaligned<int16_t>(input + i * 2)) / 32768.0f;
            break;
        case SampleKind::Int24: {
            const BYTE* sample = input + i * 3;
            int32_t value = (static_cast<int32_t>(sample[0]) << 8) |
                (static_cast<int32_t>(sample[1]) << 16) |
                (static_cast<int32_t>(sample[2]) << 24);
            output[i] = static_cast<float>(value) / 2147483648.0f;
            break;
        }
        case SampleKind::Int32:
            output[i] = static_cast<float>(ReadUnaligned<int32_t>(input + i * 4)) / 2147483648.0f;
            break;
        }
    }
}

namespace {
float SoftClipSample(float sample) {
    const float magnitude = std::abs(sample);
    if (magnitude <= 0.8f) {
        return sample;
    }
    const float sign = sample < 0 ? -1.0f : 1.0f;
    if (magnitude >= 1.2f) {
        return sign;
    }
    const float delta = magnitude - 0.8f;
    return sign * (0.8f + delta - delta * delta / 0.8f);
}
}

void ConvertFromFloat(const float* input, size_t frames, const PcmFormat& format, BYTE* output) {
    const size_t samples = frames * format.channels;
    for (size_t i = 0; i < samples; ++i) {
        const float value = SoftClipSample(input[i]);
        switch (format.kind) {
        case SampleKind::Float32: {
            const float converted = value;
            std::memcpy(output + i * 4, &converted, sizeof(converted));
            break;
        }
        case SampleKind::Int16: {
            const int16_t converted = static_cast<int16_t>(std::lround(value * 32767.0f));
            std::memcpy(output + i * 2, &converted, sizeof(converted));
            break;
        }
        case SampleKind::Int24: {
            const int32_t converted = static_cast<int32_t>(std::lround(value * 8388607.0f));
            output[i * 3] = static_cast<BYTE>(converted & 0xff);
            output[i * 3 + 1] = static_cast<BYTE>((converted >> 8) & 0xff);
            output[i * 3 + 2] = static_cast<BYTE>((converted >> 16) & 0xff);
            break;
        }
        case SampleKind::Int32: {
            const int32_t converted = static_cast<int32_t>(std::llround(static_cast<double>(value) * 2147483647.0));
            std::memcpy(output + i * 4, &converted, sizeof(converted));
            break;
        }
        }
    }
}

AudioRingBuffer::AudioRingBuffer(size_t capacityFrames, uint16_t channels) {
    Reset(capacityFrames, channels);
}

void AudioRingBuffer::Reset(size_t capacityFrames, uint16_t channels) {
    data_.assign(capacityFrames * channels, 0);
    capacityFrames_ = capacityFrames;
    channels_ = channels;
    readFrame_ = 0;
    writeFrame_ = 0;
    countFrames_ = 0;
}

void AudioRingBuffer::Drop(size_t frames) {
    const size_t dropped = std::min(frames, countFrames_);
    readFrame_ = (readFrame_ + dropped) % capacityFrames_;
    countFrames_ -= dropped;
}

void AudioRingBuffer::Write(const float* input, size_t frames) {
    if (frames == 0 || capacityFrames_ == 0) {
        return;
    }
    if (frames >= capacityFrames_) {
        input += (frames - capacityFrames_) * channels_;
        frames = capacityFrames_;
        readFrame_ = 0;
        writeFrame_ = 0;
        countFrames_ = 0;
    }
    if (frames > capacityFrames_ - countFrames_) {
        Drop(frames - (capacityFrames_ - countFrames_));
    }

    const size_t first = std::min(frames, capacityFrames_ - writeFrame_);
    std::memcpy(data_.data() + writeFrame_ * channels_, input, first * channels_ * sizeof(float));
    const size_t second = frames - first;
    if (second > 0) {
        std::memcpy(data_.data(), input + first * channels_, second * channels_ * sizeof(float));
    }
    writeFrame_ = (writeFrame_ + frames) % capacityFrames_;
    countFrames_ += frames;
}

size_t AudioRingBuffer::Read(float* output, size_t frames) {
    const size_t available = std::min(frames, countFrames_);
    const size_t first = std::min(available, capacityFrames_ - readFrame_);
    std::memcpy(output, data_.data() + readFrame_ * channels_, first * channels_ * sizeof(float));
    const size_t second = available - first;
    if (second > 0) {
        std::memcpy(output + first * channels_, data_.data(), second * channels_ * sizeof(float));
    }
    readFrame_ = (readFrame_ + available) % capacityFrames_;
    countFrames_ -= available;
    if (available < frames) {
        std::memset(output + available * channels_, 0, (frames - available) * channels_ * sizeof(float));
    }
    return available;
}

size_t AudioRingBuffer::Size() const {
    return countFrames_;
}

void StreamConverter::Reset(uint32_t inputRate, uint16_t inputChannels, uint32_t outputRate, uint16_t outputChannels) {
    inputRate_ = inputRate;
    inputChannels_ = inputChannels;
    outputRate_ = outputRate;
    outputChannels_ = outputChannels;
    pending_.clear();
    converted_.clear();
    position_ = 0;
}

void StreamConverter::Process(const float* input, size_t inputFrames, double speed, AudioRingBuffer& output) {
    const size_t oldFrames = pending_.size() / outputChannels_;
    pending_.resize((oldFrames + inputFrames) * outputChannels_);
    for (size_t frame = 0; frame < inputFrames; ++frame) {
        const float* source = input + frame * inputChannels_;
        float* destination = pending_.data() + (oldFrames + frame) * outputChannels_;
        if (inputChannels_ == outputChannels_) {
            std::memcpy(destination, source, outputChannels_ * sizeof(float));
        } else if (inputChannels_ == 1) {
            std::fill(destination, destination + outputChannels_, source[0]);
        } else if (outputChannels_ == 1) {
            float sum = 0;
            for (uint16_t channel = 0; channel < inputChannels_; ++channel) {
                sum += source[channel];
            }
            destination[0] = sum / inputChannels_;
        } else {
            const uint16_t copied = std::min(inputChannels_, outputChannels_);
            std::memcpy(destination, source, copied * sizeof(float));
            std::fill(destination + copied, destination + outputChannels_, 0);
        }
    }

    converted_.clear();
    const size_t totalFrames = pending_.size() / outputChannels_;
    const double step = static_cast<double>(inputRate_) / outputRate_ * speed;
    while (position_ + 1 < totalFrames) {
        const size_t left = static_cast<size_t>(position_);
        const float fraction = static_cast<float>(position_ - left);
        for (uint16_t channel = 0; channel < outputChannels_; ++channel) {
            const float a = pending_[left * outputChannels_ + channel];
            const float b = pending_[(left + 1) * outputChannels_ + channel];
            converted_.push_back(a + (b - a) * fraction);
        }
        position_ += step;
    }

    const size_t consumed = std::min(static_cast<size_t>(position_), totalFrames);
    position_ -= consumed;
    const size_t remainingSamples = pending_.size() - consumed * outputChannels_;
    if (consumed > 0) {
        std::memmove(pending_.data(), pending_.data() + consumed * outputChannels_, remainingSamples * sizeof(float));
    }
    pending_.resize(remainingSamples);
    output.Write(converted_.data(), converted_.size() / outputChannels_);
}

void NoiseGate::Reset(uint32_t sampleRate, uint16_t channels) {
    channels_ = channels;
    threshold_ = std::pow(10.0f, -45.0f / 20.0f);
    attackStep_ = 1.0f / std::max(1.0f, sampleRate * 0.005f);
    releaseStep_ = 1.0f / std::max(1.0f, sampleRate * 0.150f);
    holdFrames_ = sampleRate * 80 / 1000;
    holdRemaining_ = 0;
    gain_ = 1;
}

void NoiseGate::Process(float* samples, size_t frames, bool enabled) {
    if (!enabled && gain_ == 1.0f) {
        return;
    }
    for (size_t frame = 0; frame < frames; ++frame) {
        float target = 1.0f;
        if (enabled) {
            float peak = 0;
            for (uint16_t channel = 0; channel < channels_; ++channel) {
                peak = std::max(peak, std::abs(samples[frame * channels_ + channel]));
            }
            if (peak >= threshold_) {
                holdRemaining_ = holdFrames_;
            } else if (holdRemaining_ > 0) {
                --holdRemaining_;
            } else {
                target = 0;
            }
        }
        const float step = target > gain_ ? attackStep_ : releaseStep_;
        if (target > gain_) {
            gain_ = std::min(target, gain_ + step);
        } else if (target < gain_) {
            gain_ = std::max(target, gain_ - step);
        }
        for (uint16_t channel = 0; channel < channels_; ++channel) {
            samples[frame * channels_ + channel] *= gain_;
        }
    }
}
