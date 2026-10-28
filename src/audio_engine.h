#pragma once

#include <string>
#include <vector>

struct InputDevice {
    std::wstring id;
    std::wstring name;
};

enum class AudioState {
    Stopped,
    Running,
    Error,
};

using AudioEventCallback = void (*)(AudioState state, const std::wstring& message, void* context);

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    bool ListInputs(std::vector<InputDevice>& devices, std::wstring& error);
    bool Start(const std::wstring& inputId, AudioEventCallback callback, void* context, std::wstring& error);
    void Stop() noexcept;
    void SetVolume(float normalized) noexcept;
    void SetNoiseGateEnabled(bool enabled) noexcept;

private:
    struct Impl;
    Impl* impl_;
};
