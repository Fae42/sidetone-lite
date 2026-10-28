#include "audio_engine.h"

#include "dsp.h"

#include <windows.h>
#include <audioclient.h>
#include <avrt.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <propidl.h>
#include <propvarutil.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <thread>
#include <utility>

namespace {
template <typename T>
class ComPtr {
public:
    ComPtr() = default;
    ~ComPtr() { Reset(); }
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    T* Get() const { return value_; }
    class Address {
    public:
        explicit Address(ComPtr* owner) : owner_(owner) {}
        operator T**() const { return &owner_->value_; }
        operator void**() const { return reinterpret_cast<void**>(&owner_->value_); }

    private:
        ComPtr* owner_;
    };

    Address AddressOf() {
        Reset();
        return Address(this);
    }
    T* operator->() const { return value_; }
    explicit operator bool() const { return value_ != nullptr; }

    void Reset() {
        if (value_) {
            value_->Release();
            value_ = nullptr;
        }
    }

private:
    T* value_ = nullptr;
};

class ComApartment {
public:
    ComApartment() {
        result_ = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        uninitialize_ = SUCCEEDED(result_);
    }
    ~ComApartment() {
        if (uninitialize_) {
            CoUninitialize();
        }
    }
    bool usable() const { return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE; }
    HRESULT result() const { return result_; }

private:
    HRESULT result_ = E_FAIL;
    bool uninitialize_ = false;
};

std::wstring HresultText(HRESULT result) {
    wchar_t text[32];
    swprintf(text, 32, L"0x%08lX", static_cast<unsigned long>(result));
    return text;
}

std::wstring MakeError(const wchar_t* operation, HRESULT result) {
    return std::wstring(operation) + L"（" + HresultText(result) + L"）";
}

struct StreamResources {
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice> captureDevice;
    ComPtr<IMMDevice> renderDevice;
    ComPtr<IAudioClient3> captureClient;
    ComPtr<IAudioClient3> renderClient;
    ComPtr<IAudioCaptureClient> capture;
    ComPtr<IAudioRenderClient> render;
    WAVEFORMATEX* captureFormat = nullptr;
    WAVEFORMATEX* renderFormat = nullptr;
    HANDLE captureEvent = nullptr;
    HANDLE renderEvent = nullptr;
    HANDLE mmcss = nullptr;
    bool captureStarted = false;
    bool renderStarted = false;

    ~StreamResources() {
        if (captureStarted) {
            captureClient->Stop();
        }
        if (renderStarted) {
            renderClient->Stop();
        }
        if (mmcss) {
            AvRevertMmThreadCharacteristics(mmcss);
        }
        if (captureEvent) {
            CloseHandle(captureEvent);
        }
        if (renderEvent) {
            CloseHandle(renderEvent);
        }
        CoTaskMemFree(captureFormat);
        CoTaskMemFree(renderFormat);
    }
};

UINT32 QueryLowestPeriod(IAudioClient3* client, const WAVEFORMATEX* format) {
    AudioClientProperties properties{};
    properties.cbSize = sizeof(properties);
    properties.bIsOffload = FALSE;
    properties.eCategory = AudioCategory_GameChat;
    properties.Options = AUDCLNT_STREAMOPTIONS_NONE;
    if (FAILED(client->SetClientProperties(&properties))) {
        return 0;
    }

    UINT32 defaultFrames = 0;
    UINT32 fundamentalFrames = 0;
    UINT32 minimumFrames = 0;
    UINT32 maximumFrames = 0;
    if (FAILED(client->GetSharedModeEnginePeriod(
            format, &defaultFrames, &fundamentalFrames, &minimumFrames, &maximumFrames)) ||
        fundamentalFrames == 0 || minimumFrames == 0 || maximumFrames < minimumFrames) {
        return 0;
    }
    UINT32 period = (minimumFrames + fundamentalFrames - 1) / fundamentalFrames * fundamentalFrames;
    if (period > maximumFrames) {
        period = maximumFrames / fundamentalFrames * fundamentalFrames;
    }
    return period >= minimumFrames && period <= maximumFrames ? period : 0;
}

HRESULT InitializeAudioClient(
    IMMDevice* device,
    ComPtr<IAudioClient3>& client,
    const WAVEFORMATEX* format,
    UINT32& periodFrames) {
    periodFrames = QueryLowestPeriod(client.Get(), format);
    HRESULT result = S_OK;
    if (periodFrames > 0) {
        result = client->InitializeSharedAudioStream(
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK, periodFrames, format, nullptr);
        if (SUCCEEDED(result)) {
            return result;
        }
    }

    client.Reset();
    periodFrames = 0;
    result = device->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, nullptr, client.AddressOf());
    if (FAILED(result)) {
        return result;
    }
    return client->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
        0,
        0,
        format,
        nullptr);
}
}

struct AudioEngine::Impl {
    std::thread worker;
    HANDLE stopEvent = nullptr;
    std::mutex mutex;
    std::condition_variable startCondition;
    bool startFinished = false;
    bool startSucceeded = false;
    std::wstring startError;
    AudioEventCallback callback = nullptr;
    void* context = nullptr;
    std::atomic<float> volume{0.5f};
    std::atomic<bool> noiseGate{false};
    std::atomic<bool> running{false};

    ~Impl() { Stop(); }

    void SignalStart(bool succeeded, const std::wstring& error = {}) {
        {
            std::lock_guard lock(mutex);
            startSucceeded = succeeded;
            startError = error;
            startFinished = true;
        }
        startCondition.notify_one();
    }

    void Notify(AudioState state, const std::wstring& message) {
        if (callback) {
            callback(state, message, context);
        }
    }

    bool ListInputs(std::vector<InputDevice>& devices, std::wstring& error) {
        devices.clear();
        ComApartment apartment;
        if (!apartment.usable()) {
            error = MakeError(L"无法初始化音频组件", apartment.result());
            return false;
        }

        ComPtr<IMMDeviceEnumerator> enumerator;
        HRESULT result = CoCreateInstance(
            __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator), enumerator.AddressOf());
        if (FAILED(result)) {
            error = MakeError(L"无法访问音频设备", result);
            return false;
        }

        ComPtr<IMMDeviceCollection> collection;
        result = enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, collection.AddressOf());
        if (FAILED(result)) {
            error = MakeError(L"无法枚举麦克风", result);
            return false;
        }

        UINT count = 0;
        result = collection->GetCount(&count);
        if (FAILED(result)) {
            error = MakeError(L"无法读取麦克风数量", result);
            return false;
        }
        for (UINT index = 0; index < count; ++index) {
            ComPtr<IMMDevice> device;
            result = collection->Item(index, device.AddressOf());
            if (FAILED(result)) {
                continue;
            }

            wchar_t* id = nullptr;
            result = device->GetId(&id);
            if (FAILED(result)) {
                continue;
            }

            ComPtr<IPropertyStore> properties;
            std::wstring name = L"麦克风";
            if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, properties.AddressOf()))) {
                PROPVARIANT value;
                PropVariantInit(&value);
                if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value)) && value.pwszVal) {
                    name = value.pwszVal;
                }
                PropVariantClear(&value);
            }
            devices.push_back({id, name});
            CoTaskMemFree(id);
        }
        return true;
    }

    bool Start(const std::wstring& inputId, AudioEventCallback eventCallback, void* eventContext, std::wstring& error) {
        Stop();
        if (inputId.empty()) {
            error = L"请选择麦克风";
            return false;
        }
        stopEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!stopEvent) {
            error = L"无法创建音频停止事件";
            return false;
        }

        callback = eventCallback;
        context = eventContext;
        {
            std::lock_guard lock(mutex);
            startFinished = false;
            startSucceeded = false;
            startError.clear();
        }
        try {
            worker = std::thread([this, inputId] { Run(inputId); });
        } catch (...) {
            CloseHandle(stopEvent);
            stopEvent = nullptr;
            error = L"无法创建音频线程";
            return false;
        }

        {
            std::unique_lock lock(mutex);
            startCondition.wait(lock, [this] { return startFinished; });
            error = startError;
        }
        if (!startSucceeded) {
            Stop();
            return false;
        }
        return true;
    }

    void Stop() noexcept {
        if (stopEvent) {
            SetEvent(stopEvent);
        }
        if (worker.joinable()) {
            worker.join();
        }
        if (stopEvent) {
            CloseHandle(stopEvent);
            stopEvent = nullptr;
        }
        running.store(false);
    }

    void Run(const std::wstring& inputId) {
        ComApartment apartment;
        if (!apartment.usable()) {
            SignalStart(false, MakeError(L"无法初始化音频线程", apartment.result()));
            return;
        }

        StreamResources resources;
        PcmFormat captureFormat{};
        PcmFormat renderFormat{};
        UINT32 renderBufferFrames = 0;
        HRESULT result = CoCreateInstance(
            __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator), resources.enumerator.AddressOf());
        if (FAILED(result)) {
            SignalStart(false, MakeError(L"无法访问音频设备", result));
            return;
        }
        result = resources.enumerator->GetDevice(inputId.c_str(), resources.captureDevice.AddressOf());
        if (FAILED(result)) {
            SignalStart(false, MakeError(L"所选麦克风不可用", result));
            return;
        }
        result = resources.enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, resources.renderDevice.AddressOf());
        if (FAILED(result)) {
            SignalStart(false, MakeError(L"没有可用的默认输出设备", result));
            return;
        }
        result = resources.captureDevice->Activate(
            __uuidof(IAudioClient3), CLSCTX_ALL, nullptr, resources.captureClient.AddressOf());
        if (FAILED(result)) {
            SignalStart(false, MakeError(L"无法打开麦克风", result));
            return;
        }
        result = resources.renderDevice->Activate(
            __uuidof(IAudioClient3), CLSCTX_ALL, nullptr, resources.renderClient.AddressOf());
        if (FAILED(result)) {
            SignalStart(false, MakeError(L"无法打开输出设备", result));
            return;
        }
        result = resources.captureClient->GetMixFormat(&resources.captureFormat);
        if (FAILED(result)) {
            SignalStart(false, MakeError(L"无法读取麦克风格式", result));
            return;
        }
        result = resources.renderClient->GetMixFormat(&resources.renderFormat);
        if (FAILED(result)) {
            SignalStart(false, MakeError(L"无法读取输出格式", result));
            return;
        }
        if (!DescribePcmFormat(*resources.captureFormat, captureFormat) ||
            !DescribePcmFormat(*resources.renderFormat, renderFormat)) {
            SignalStart(false, L"遇到了不支持的音频格式");
            return;
        }

        UINT32 capturePeriod = 0;
        UINT32 renderPeriod = 0;
        result = InitializeAudioClient(
            resources.captureDevice.Get(), resources.captureClient, resources.captureFormat, capturePeriod);
        if (FAILED(result)) {
            SignalStart(false, MakeError(L"无法初始化麦克风共享流", result));
            return;
        }
        result = InitializeAudioClient(
            resources.renderDevice.Get(), resources.renderClient, resources.renderFormat, renderPeriod);
        if (FAILED(result)) {
            SignalStart(false, MakeError(L"无法初始化输出共享流", result));
            return;
        }

        resources.captureEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        resources.renderEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!resources.captureEvent || !resources.renderEvent) {
            SignalStart(false, L"无法创建音频事件");
            return;
        }
        result = resources.captureClient->SetEventHandle(resources.captureEvent);
        if (FAILED(result)) {
            SignalStart(false, MakeError(L"无法设置麦克风事件", result));
            return;
        }
        result = resources.renderClient->SetEventHandle(resources.renderEvent);
        if (FAILED(result)) {
            SignalStart(false, MakeError(L"无法设置输出事件", result));
            return;
        }
        result = resources.captureClient->GetService(__uuidof(IAudioCaptureClient), resources.capture.AddressOf());
        if (FAILED(result)) {
            SignalStart(false, MakeError(L"无法获取采集服务", result));
            return;
        }
        result = resources.renderClient->GetService(__uuidof(IAudioRenderClient), resources.render.AddressOf());
        if (FAILED(result)) {
            SignalStart(false, MakeError(L"无法获取播放服务", result));
            return;
        }
        result = resources.renderClient->GetBufferSize(&renderBufferFrames);
        if (FAILED(result) || renderBufferFrames == 0) {
            SignalStart(false, MakeError(L"无法读取输出缓冲", result));
            return;
        }

        BYTE* renderData = nullptr;
        result = resources.render->GetBuffer(renderBufferFrames, &renderData);
        if (FAILED(result)) {
            SignalStart(false, MakeError(L"无法预填输出缓冲", result));
            return;
        }
        result = resources.render->ReleaseBuffer(renderBufferFrames, AUDCLNT_BUFFERFLAGS_SILENT);
        if (FAILED(result)) {
            SignalStart(false, MakeError(L"无法提交输出缓冲", result));
            return;
        }

        AudioRingBuffer ring(renderFormat.sampleRate / 4, renderFormat.channels);
        const size_t targetFrames = std::max({
            static_cast<size_t>(renderFormat.sampleRate) * 5 / 1000,
            static_cast<size_t>(capturePeriod),
            static_cast<size_t>(renderPeriod)});
        StreamConverter converter;
        converter.Reset(
            captureFormat.sampleRate, captureFormat.channels,
            renderFormat.sampleRate, renderFormat.channels);
        NoiseGate gate;
        gate.Reset(renderFormat.sampleRate, renderFormat.channels);
        std::vector<float> captureFloat;
        std::vector<float> renderFloat(renderBufferFrames * renderFormat.channels);
        bool primed = false;
        float renderGain = volume.load(std::memory_order_relaxed);

        auto pumpCapture = [&]() -> HRESULT {
            UINT32 packetFrames = 0;
            HRESULT captureResult = resources.capture->GetNextPacketSize(&packetFrames);
            while (SUCCEEDED(captureResult) && packetFrames > 0) {
                BYTE* data = nullptr;
                UINT32 frames = 0;
                DWORD flags = 0;
                captureResult = resources.capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
                if (FAILED(captureResult)) {
                    break;
                }
                if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) {
                    converter.Reset(
                        captureFormat.sampleRate, captureFormat.channels,
                        renderFormat.sampleRate, renderFormat.channels);
                }
                captureFloat.resize(frames * captureFormat.channels);
                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                    std::fill(captureFloat.begin(), captureFloat.end(), 0);
                } else {
                    ConvertToFloat(data, frames, captureFormat, captureFloat.data());
                }
                const double level = static_cast<double>(ring.Size()) / std::max<size_t>(1, targetFrames);
                const double speed = std::clamp(1.0 + (level - 1.0) * 0.005, 0.98, 1.02);
                converter.Process(captureFloat.data(), frames, speed, ring);
                captureResult = resources.capture->ReleaseBuffer(frames);
                if (FAILED(captureResult)) {
                    break;
                }
                captureResult = resources.capture->GetNextPacketSize(&packetFrames);
            }
            return captureResult;
        };

        auto pumpRender = [&]() -> HRESULT {
            UINT32 padding = 0;
            HRESULT renderResult = resources.renderClient->GetCurrentPadding(&padding);
            if (FAILED(renderResult) || padding >= renderBufferFrames) {
                return renderResult;
            }
            const UINT32 frames = renderBufferFrames - padding;
            renderResult = resources.render->GetBuffer(frames, &renderData);
            if (FAILED(renderResult)) {
                return renderResult;
            }
            size_t read = 0;
            if (primed || ring.Size() >= targetFrames) {
                primed = true;
                read = ring.Read(renderFloat.data(), frames);
                if (read == 0) {
                    primed = false;
                }
            } else {
                std::fill(renderFloat.begin(), renderFloat.begin() + frames * renderFormat.channels, 0);
            }
            gate.Process(renderFloat.data(), frames, noiseGate.load(std::memory_order_relaxed));
            const float targetGain = volume.load(std::memory_order_relaxed);
            const float gainStep = 1.0f / std::max(1.0f, renderFormat.sampleRate * 0.005f);
            for (UINT32 frame = 0; frame < frames; ++frame) {
                if (renderGain < targetGain) {
                    renderGain = std::min(targetGain, renderGain + gainStep);
                } else if (renderGain > targetGain) {
                    renderGain = std::max(targetGain, renderGain - gainStep);
                }
                for (uint16_t channel = 0; channel < renderFormat.channels; ++channel) {
                    renderFloat[frame * renderFormat.channels + channel] *= renderGain;
                }
            }
            ConvertFromFloat(renderFloat.data(), frames, renderFormat, renderData);
            return resources.render->ReleaseBuffer(frames, read == 0 ? AUDCLNT_BUFFERFLAGS_SILENT : 0);
        };

        result = resources.renderClient->Start();
        if (FAILED(result)) {
            SignalStart(false, MakeError(L"无法启动输出", result));
            return;
        }
        resources.renderStarted = true;
        result = resources.captureClient->Start();
        if (FAILED(result)) {
            SignalStart(false, MakeError(L"无法启动麦克风", result));
            return;
        }
        resources.captureStarted = true;

        DWORD taskIndex = 0;
        resources.mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
        running.store(true);
        SignalStart(true);
        std::wstring runningMessage = L"正在监听 · 默认周期";
        if (renderPeriod > 0) {
            wchar_t text[64];
            swprintf(text, 64, L"正在监听 · %.2f ms", 1000.0 * renderPeriod / renderFormat.sampleRate);
            runningMessage = text;
        }
        Notify(AudioState::Running, runningMessage);

        HANDLE events[] = {stopEvent, resources.captureEvent, resources.renderEvent};
        std::wstring runtimeError;
        for (;;) {
            const DWORD wait = WaitForMultipleObjects(3, events, FALSE, INFINITE);
            if (wait == WAIT_OBJECT_0) {
                break;
            }
            if (wait == WAIT_OBJECT_0 + 1 || wait == WAIT_OBJECT_0 + 2) {
                result = pumpCapture();
                if (SUCCEEDED(result)) {
                    result = pumpRender();
                }
                if (FAILED(result)) {
                    runtimeError = MakeError(L"音频设备已断开或流已失效", result);
                    break;
                }
                continue;
            }
            runtimeError = L"音频等待失败";
            break;
        }
        running.store(false);
        if (runtimeError.empty()) {
            Notify(AudioState::Stopped, L"已停止");
        } else {
            Notify(AudioState::Error, runtimeError);
        }
    }
};

AudioEngine::AudioEngine() : impl_(new Impl) {
}

AudioEngine::~AudioEngine() {
    delete impl_;
}

bool AudioEngine::ListInputs(std::vector<InputDevice>& devices, std::wstring& error) {
    return impl_->ListInputs(devices, error);
}

bool AudioEngine::Start(const std::wstring& inputId, AudioEventCallback callback, void* context, std::wstring& error) {
    return impl_->Start(inputId, callback, context, error);
}

void AudioEngine::Stop() noexcept {
    impl_->Stop();
}

void AudioEngine::SetVolume(float normalized) noexcept {
    impl_->volume.store(std::clamp(normalized, 0.0f, 8.0f), std::memory_order_relaxed);
}

void AudioEngine::SetNoiseGateEnabled(bool enabled) noexcept {
    impl_->noiseGate.store(enabled, std::memory_order_relaxed);
}
