#pragma once

#include "audio_engine.h"

#include <windows.h>
#include <mmdeviceapi.h>
#include <string>
#include <vector>

class AppWindow {
public:
    AppWindow() = default;
    ~AppWindow();
    AppWindow(const AppWindow&) = delete;
    AppWindow& operator=(const AppWindow&) = delete;

    bool Create(HINSTANCE instance);
    int Run(int showCommand);

private:
    class DeviceNotifier;

    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static void AudioEvent(AudioState state, const std::wstring& message, void* context);

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void OnCreate();
    void OnCommand(int id);
    void OnDestroy();
    void OnDeviceChange(DWORD kind, const wchar_t* id);
    void RefreshDevices();
    void ToggleListening();
    void StopListening();
    void SetRunningUi(bool running);
    void SetStatus(const std::wstring& text, int kind);
    void UpdateVolume();
    void UpdateFonts();
    void Layout();
    void DrawItem(const DRAWITEMSTRUCT& item);
    int Scale(int value) const;

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    HWND deviceCombo_ = nullptr;
    HWND volumeSlider_ = nullptr;
    HWND volumeValue_ = nullptr;
    HWND gateCheck_ = nullptr;
    HWND startButton_ = nullptr;
    HWND statusText_ = nullptr;
    HWND inputLabel_ = nullptr;
    HWND volumeLabel_ = nullptr;
    HWND footerText_ = nullptr;
    HFONT font_ = nullptr;
    HFONT smallFont_ = nullptr;
    HBRUSH backgroundBrush_ = nullptr;
    HBRUSH controlBrush_ = nullptr;
    AudioEngine engine_;
    DeviceNotifier* notifier_ = nullptr;
    std::vector<InputDevice> devices_;
    std::wstring activeInputId_;
    std::wstring rememberedInputId_;
    UINT dpi_ = 96;
    bool running_ = false;
    bool gateEnabled_ = false;
    int statusKind_ = 0;
};
