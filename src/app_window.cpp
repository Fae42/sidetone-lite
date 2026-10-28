#include "app_window.h"
#include "resource.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>

#include <algorithm>
#include <memory>

namespace {
constexpr wchar_t kClassName[] = L"JiantingWindow";
constexpr UINT kAudioMessage = WM_APP + 1;
constexpr UINT kDeviceMessage = WM_APP + 2;
constexpr int kDevice = 1001;
constexpr int kVolume = 1002;
constexpr int kGate = 1003;
constexpr int kStart = 1004;
constexpr int kStatus = 1005;
constexpr int kInputLabel = 1006;
constexpr int kVolumeLabel = 1007;
constexpr int kVolumeValue = 1008;
constexpr int kFooter = 1009;
constexpr DWORD kDeviceState = 1;
constexpr DWORD kDeviceAdded = 2;
constexpr DWORD kDeviceRemoved = 3;
constexpr DWORD kDefaultChanged = 4;

constexpr COLORREF kBackground = RGB(15, 17, 16);
constexpr COLORREF kControl = RGB(27, 31, 28);
constexpr COLORREF kText = RGB(231, 235, 232);
constexpr COLORREF kMuted = RGB(139, 150, 143);
constexpr COLORREF kAccent = RGB(124, 226, 167);
constexpr COLORREF kError = RGB(242, 129, 119);
constexpr COLORREF kBorder = RGB(53, 63, 56);

struct AudioMessage {
    AudioState state;
    std::wstring text;
};

struct DeviceChangeMessage {
    DWORD kind;
    std::wstring id;
};

}

class AppWindow::DeviceNotifier : public IMMNotificationClient {
public:
    explicit DeviceNotifier(HWND window) : window_(window) {}

    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(InterlockedIncrement(&references_));
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG count = static_cast<ULONG>(InterlockedDecrement(&references_));
        if (count == 0) {
            delete this;
        }
        return count;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** object) override {
        if (!object) {
            return E_POINTER;
        }
        if (id == __uuidof(IUnknown) || id == __uuidof(IMMNotificationClient)) {
            *object = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        *object = nullptr;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR id, DWORD) override {
        Post(kDeviceState, id);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR id) override {
        Post(kDeviceAdded, id);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR id) override {
        Post(kDeviceRemoved, id);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR id) override {
        if (flow == eRender && role == eMultimedia) {
            Post(kDefaultChanged, id);
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override {
        return S_OK;
    }

private:
    void Post(DWORD kind, LPCWSTR id) {
        auto message = std::make_unique<DeviceChangeMessage>(DeviceChangeMessage{kind, id ? id : L""});
        if (PostMessageW(window_, kDeviceMessage, 0, reinterpret_cast<LPARAM>(message.get()))) {
            message.release();
        }
    }

    HWND window_;
    LONG references_ = 1;
};

namespace {
HWND MakeControl(
    DWORD extendedStyle,
    const wchar_t* className,
    const wchar_t* text,
    DWORD style,
    int id,
    HWND parent,
    HINSTANCE instance) {
    return CreateWindowExW(
        extendedStyle, className, text, WS_CHILD | WS_VISIBLE | style,
        0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
}
}

AppWindow::~AppWindow() {
    engine_.Stop();
    DeleteObject(font_);
    DeleteObject(smallFont_);
    DeleteObject(backgroundBrush_);
    DeleteObject(controlBrush_);
}

bool AppWindow::Create(HINSTANCE instance) {
    instance_ = instance;
    WNDCLASSW windowClass{};
    windowClass.hInstance = instance_;
    windowClass.lpszClassName = kClassName;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_APPICON));
    if (!RegisterClassW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    dpi_ = GetDpiForSystem();
    RECT rectangle{0, 0, Scale(360), Scale(280)};
    AdjustWindowRectExForDpi(
        &rectangle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        FALSE,
        0,
        dpi_);
    window_ = CreateWindowExW(
        0,
        kClassName,
        L"监听",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rectangle.right - rectangle.left,
        rectangle.bottom - rectangle.top,
        nullptr,
        nullptr,
        instance_,
        this);
    return window_ != nullptr;
}

int AppWindow::Run(int showCommand) {
    ShowWindow(window_, showCommand);
    UpdateWindow(window_);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK AppWindow::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    AppWindow* self = reinterpret_cast<AppWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<AppWindow*>(create->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self) {
        return self->HandleMessage(message, wParam, lParam);
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void AppWindow::AudioEvent(AudioState state, const std::wstring& message, void* context) {
    auto* self = static_cast<AppWindow*>(context);
    auto event = std::make_unique<AudioMessage>(AudioMessage{state, message});
    if (PostMessageW(self->window_, kAudioMessage, 0, reinterpret_cast<LPARAM>(event.get()))) {
        event.release();
    }
}

LRESULT AppWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        OnCreate();
        return 0;
    case WM_COMMAND:
        OnCommand(LOWORD(wParam));
        return 0;
    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lParam) == volumeSlider_) {
            UpdateVolume();
        }
        return 0;
    case kAudioMessage: {
        std::unique_ptr<AudioMessage> event(reinterpret_cast<AudioMessage*>(lParam));
        if (event->state == AudioState::Error) {
            engine_.Stop();
            running_ = false;
            SetRunningUi(false);
            SetStatus(event->text, 2);
        } else if (event->state == AudioState::Running) {
            running_ = true;
            SetRunningUi(true);
            SetStatus(event->text, 1);
        } else {
            running_ = false;
            SetRunningUi(false);
            if (statusKind_ != 2) {
                SetStatus(L"已停止", 0);
            }
        }
        return 0;
    }
    case kDeviceMessage: {
        std::unique_ptr<DeviceChangeMessage> event(reinterpret_cast<DeviceChangeMessage*>(lParam));
        OnDeviceChange(event->kind, event->id.c_str());
        return 0;
    }
    case WM_ERASEBKGND: {
        RECT rectangle;
        GetClientRect(window_, &rectangle);
        FillRect(reinterpret_cast<HDC>(wParam), &rectangle, backgroundBrush_);
        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window_, &paint);
        const COLORREF color = statusKind_ == 1 ? kAccent : statusKind_ == 2 ? kError : kMuted;
        HBRUSH brush = CreateSolidBrush(color);
        RECT dot{Scale(24), Scale(23), Scale(32), Scale(31)};
        FillRect(dc, &dot, brush);
        DeleteObject(brush);
        HPEN pen = CreatePen(PS_SOLID, 1, kBorder);
        HGDIOBJ oldPen = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        MoveToEx(dc, Scale(24), Scale(49), nullptr);
        LineTo(dc, Scale(336), Scale(49));
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(pen);
        EndPaint(window_, &paint);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, TRANSPARENT);
        const int id = GetDlgCtrlID(reinterpret_cast<HWND>(lParam));
        SetTextColor(dc, id == kFooter || id == kInputLabel || id == kVolumeLabel ? kMuted : kText);
        return reinterpret_cast<LRESULT>(backgroundBrush_);
    }
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkColor(dc, kControl);
        SetTextColor(dc, kText);
        return reinterpret_cast<LRESULT>(controlBrush_);
    }
    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, kText);
        return reinterpret_cast<LRESULT>(backgroundBrush_);
    }
    case WM_NOTIFY: {
        auto& draw = *reinterpret_cast<NMCUSTOMDRAW*>(lParam);
        if (draw.hdr.hwndFrom != volumeSlider_ || draw.hdr.code != NM_CUSTOMDRAW) {
            return 0;
        }
        if (draw.dwDrawStage == CDDS_PREPAINT) {
            return CDRF_NOTIFYITEMDRAW;
        }
        if (draw.dwDrawStage != CDDS_ITEMPREPAINT) {
            return CDRF_DODEFAULT;
        }
        if (draw.dwItemSpec == TBCD_THUMB) {
            HBRUSH brush = CreateSolidBrush(kAccent);
            HPEN pen = CreatePen(PS_SOLID, 1, kAccent);
            HGDIOBJ oldBrush = SelectObject(draw.hdc, brush);
            HGDIOBJ oldPen = SelectObject(draw.hdc, pen);
            RoundRect(draw.hdc, draw.rc.left, draw.rc.top, draw.rc.right, draw.rc.bottom, Scale(6), Scale(6));
            SelectObject(draw.hdc, oldBrush);
            SelectObject(draw.hdc, oldPen);
            DeleteObject(brush);
            DeleteObject(pen);
            return CDRF_SKIPDEFAULT;
        }
        if (draw.dwItemSpec == TBCD_CHANNEL) {
            RECT thumb{};
            SendMessageW(volumeSlider_, TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>(&thumb));
            RECT track = draw.rc;
            const int centerY = (track.top + track.bottom) / 2;
            track.top = centerY - Scale(1);
            track.bottom = centerY + Scale(2);
            HBRUSH background = CreateSolidBrush(kBorder);
            FillRect(draw.hdc, &track, background);
            DeleteObject(background);
            track.right = std::min(track.right, (thumb.left + thumb.right) / 2);
            if (track.right > track.left) {
                HBRUSH played = CreateSolidBrush(kAccent);
                FillRect(draw.hdc, &track, played);
                DeleteObject(played);
            }
            return CDRF_SKIPDEFAULT;
        }
        return CDRF_DODEFAULT;
    }
    case WM_DRAWITEM:
        DrawItem(*reinterpret_cast<DRAWITEMSTRUCT*>(lParam));
        return TRUE;
    case WM_DPICHANGED: {
        const auto* suggested = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(window_, nullptr, suggested->left, suggested->top,
            suggested->right - suggested->left, suggested->bottom - suggested->top,
            SWP_NOACTIVATE | SWP_NOZORDER);
        dpi_ = HIWORD(wParam);
        UpdateFonts();
        Layout();
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(window_);
        return 0;
    case WM_DESTROY:
        OnDestroy();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window_, message, wParam, lParam);
    }
}

void AppWindow::OnCreate() {
    dpi_ = GetDpiForWindow(window_);
    backgroundBrush_ = CreateSolidBrush(kBackground);
    controlBrush_ = CreateSolidBrush(kControl);
    BOOL dark = TRUE;
    if (FAILED(DwmSetWindowAttribute(window_, 20, &dark, sizeof(dark)))) {
        DwmSetWindowAttribute(window_, 19, &dark, sizeof(dark));
    }

    statusText_ = MakeControl(0, L"STATIC", L"正在准备", SS_LEFT, kStatus, window_, instance_);
    inputLabel_ = MakeControl(0, L"STATIC", L"麦克风", SS_LEFT, kInputLabel, window_, instance_);
    volumeLabel_ = MakeControl(0, L"STATIC", L"监听音量", SS_LEFT, kVolumeLabel, window_, instance_);
    volumeValue_ = MakeControl(0, L"STATIC", L"50%", SS_RIGHT, kVolumeValue, window_, instance_);
    footerText_ = MakeControl(0, L"STATIC", L"共享低延迟 · 请使用耳机", SS_LEFT, kFooter, window_, instance_);
    deviceCombo_ = MakeControl(0, WC_COMBOBOXW, nullptr, CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, kDevice, window_, instance_);
    volumeSlider_ = MakeControl(0, TRACKBAR_CLASSW, nullptr, TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, kVolume, window_, instance_);
    gateCheck_ = MakeControl(0, L"BUTTON", L"轻量噪声门", BS_OWNERDRAW | WS_TABSTOP, kGate, window_, instance_);
    startButton_ = MakeControl(0, L"BUTTON", L"开始监听", BS_OWNERDRAW | WS_TABSTOP, kStart, window_, instance_);

    SetWindowTheme(deviceCombo_, L"DarkMode_CFD", nullptr);
    SetWindowTheme(volumeSlider_, L"DarkMode_Explorer", nullptr);
    SendMessageW(volumeSlider_, TBM_SETRANGE, TRUE, MAKELONG(0, 800));
    SendMessageW(volumeSlider_, TBM_SETPOS, TRUE, 50);
    SendMessageW(deviceCombo_, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), Scale(24));
    UpdateFonts();
    Layout();

    DeviceNotifier* notifier = new DeviceNotifier(window_);
    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT result = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
    if (SUCCEEDED(result)) {
        result = enumerator->RegisterEndpointNotificationCallback(notifier);
        enumerator->Release();
    }
    if (SUCCEEDED(result)) {
        notifier_ = notifier;
    } else {
        notifier->Release();
    }
    RefreshDevices();
    if (!notifier_) {
        SetStatus(L"无法监视音频设备变化，已禁用监听", 2);
        EnableWindow(startButton_, FALSE);
    }
}

void AppWindow::OnCommand(int id) {
    if (id == kStart) {
        ToggleListening();
    } else if (id == kGate) {
        gateEnabled_ = !gateEnabled_;
        SendMessageW(gateCheck_, BM_SETCHECK, gateEnabled_ ? BST_CHECKED : BST_UNCHECKED, 0);
        engine_.SetNoiseGateEnabled(gateEnabled_);
        InvalidateRect(gateCheck_, nullptr, FALSE);
    }
}

void AppWindow::OnDestroy() {
    if (notifier_) {
        IMMDeviceEnumerator* enumerator = nullptr;
        if (SUCCEEDED(CoCreateInstance(
                __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator)))) {
            enumerator->UnregisterEndpointNotificationCallback(notifier_);
            enumerator->Release();
        }
        notifier_->Release();
        notifier_ = nullptr;
    }
    engine_.Stop();
    running_ = false;
}

void AppWindow::OnDeviceChange(DWORD kind, const wchar_t* id) {
    if (kind == kDefaultChanged) {
        if (running_) {
            StopListening();
            SetStatus(L"默认输出已变化，请重新开始", 2);
        }
        return;
    }
    if ((kind == kDeviceRemoved || kind == kDeviceState) && running_ && activeInputId_ == id) {
        StopListening();
        SetStatus(L"所选麦克风已断开或停用", 2);
        return;
    }
    if (!running_) {
        RefreshDevices();
    }
}

void AppWindow::RefreshDevices() {
    if (running_) {
        return;
    }
    std::wstring preferred = activeInputId_;
    const LRESULT oldSelection = SendMessageW(deviceCombo_, CB_GETCURSEL, 0, 0);
    if (preferred.empty() && oldSelection != CB_ERR) {
        const LRESULT oldIndex = SendMessageW(deviceCombo_, CB_GETITEMDATA, oldSelection, 0);
        if (oldIndex != CB_ERR && static_cast<size_t>(oldIndex) < devices_.size()) {
            preferred = devices_[oldIndex].id;
        }
    }

    std::wstring error;
    if (!engine_.ListInputs(devices_, error)) {
        SetStatus(error, 2);
        EnableWindow(startButton_, FALSE);
        return;
    }
    SendMessageW(deviceCombo_, CB_RESETCONTENT, 0, 0);
    LRESULT selection = 0;
    for (size_t index = 0; index < devices_.size(); ++index) {
        const LRESULT item = SendMessageW(deviceCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(devices_[index].name.c_str()));
        SendMessageW(deviceCombo_, CB_SETITEMDATA, item, index);
        if (devices_[index].id == preferred) {
            selection = item;
        }
    }
    if (devices_.empty()) {
        EnableWindow(startButton_, FALSE);
        SetStatus(L"没有可用麦克风", 2);
    } else {
        SendMessageW(deviceCombo_, CB_SETCURSEL, selection, 0);
        EnableWindow(startButton_, notifier_ != nullptr);
        SetStatus(notifier_ ? L"就绪" : L"无法监视音频设备变化，已禁用监听", notifier_ ? 0 : 2);
    }
}

void AppWindow::ToggleListening() {
    if (running_) {
        StopListening();
        return;
    }
    if (!notifier_) {
        SetStatus(L"无法监视音频设备变化，已禁用监听", 2);
        return;
    }
    const LRESULT selection = SendMessageW(deviceCombo_, CB_GETCURSEL, 0, 0);
    if (selection == CB_ERR) {
        SetStatus(L"请选择麦克风", 2);
        return;
    }
    const LRESULT index = SendMessageW(deviceCombo_, CB_GETITEMDATA, selection, 0);
    if (index == CB_ERR || static_cast<size_t>(index) >= devices_.size()) {
        SetStatus(L"麦克风选择已失效", 2);
        return;
    }

    EnableWindow(startButton_, FALSE);
    SetWindowTextW(startButton_, L"启动中…");
    SetStatus(L"正在启动音频", 0);
    std::wstring error;
    engine_.SetVolume(static_cast<float>(SendMessageW(volumeSlider_, TBM_GETPOS, 0, 0)) / 100.0f);
    engine_.SetNoiseGateEnabled(gateEnabled_);
    activeInputId_ = devices_[index].id;
    if (engine_.Start(activeInputId_, AudioEvent, this, error)) {
        running_ = true;
        SetRunningUi(true);
        SetStatus(L"正在监听", 1);
    } else {
        activeInputId_.clear();
        running_ = false;
        SetRunningUi(false);
        SetStatus(error, 2);
    }
}

void AppWindow::StopListening() {
    EnableWindow(startButton_, FALSE);
    engine_.Stop();
    running_ = false;
    activeInputId_.clear();
    SetRunningUi(false);
    RefreshDevices();
    SetStatus(L"已停止", 0);
}

void AppWindow::SetRunningUi(bool running) {
    const bool canStart = notifier_ != nullptr && !devices_.empty();
    EnableWindow(deviceCombo_, !running && canStart);
    EnableWindow(startButton_, running || canStart);
    SetWindowTextW(startButton_, running ? L"停止监听" : L"开始监听");
    InvalidateRect(startButton_, nullptr, FALSE);
}

void AppWindow::SetStatus(const std::wstring& text, int kind) {
    statusKind_ = kind;
    SetWindowTextW(statusText_, text.c_str());
    InvalidateRect(window_, nullptr, FALSE);
}

void AppWindow::UpdateVolume() {
    const int volume = static_cast<int>(SendMessageW(volumeSlider_, TBM_GETPOS, 0, 0));
    wchar_t text[16];
    swprintf(text, 16, L"%d%%", volume);
    SetWindowTextW(volumeValue_, text);
    engine_.SetVolume(static_cast<float>(volume) / 100.0f);
    InvalidateRect(volumeSlider_, nullptr, FALSE);
}

void AppWindow::UpdateFonts() {
    HFONT oldFont = font_;
    HFONT oldSmallFont = smallFont_;
    font_ = CreateFontW(-Scale(16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    smallFont_ = CreateFontW(-Scale(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    const HWND normal[] = {statusText_, deviceCombo_, volumeValue_, gateCheck_, startButton_};
    for (HWND control : normal) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }
    const HWND small[] = {inputLabel_, volumeLabel_, footerText_};
    for (HWND control : small) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(smallFont_), TRUE);
    }
    DeleteObject(oldFont);
    DeleteObject(oldSmallFont);
}

void AppWindow::Layout() {
    SendMessageW(deviceCombo_, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), Scale(24));
    SetWindowPos(statusText_, nullptr, Scale(42), Scale(18), Scale(294), Scale(22), SWP_NOZORDER);
    SetWindowPos(inputLabel_, nullptr, Scale(24), Scale(61), Scale(312), Scale(18), SWP_NOZORDER);
    SetWindowPos(deviceCombo_, nullptr, Scale(24), Scale(82), Scale(312), Scale(180), SWP_NOZORDER);
    SetWindowPos(volumeLabel_, nullptr, Scale(24), Scale(121), Scale(180), Scale(18), SWP_NOZORDER);
    SetWindowPos(volumeValue_, nullptr, Scale(268), Scale(118), Scale(68), Scale(22), SWP_NOZORDER);
    SetWindowPos(volumeSlider_, nullptr, Scale(20), Scale(142), Scale(320), Scale(25), SWP_NOZORDER);
    SetWindowPos(gateCheck_, nullptr, Scale(24), Scale(177), Scale(180), Scale(24), SWP_NOZORDER);
    SetWindowPos(startButton_, nullptr, Scale(24), Scale(213), Scale(312), Scale(38), SWP_NOZORDER);
    SetWindowPos(footerText_, nullptr, Scale(24), Scale(259), Scale(312), Scale(16), SWP_NOZORDER);
}

void AppWindow::DrawItem(const DRAWITEMSTRUCT& item) {
    HDC dc = item.hDC;
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, font_);
    if (item.CtlID == kGate) {
        RECT box{item.rcItem.left, item.rcItem.top + Scale(4), item.rcItem.left + Scale(16), item.rcItem.top + Scale(20)};
        HBRUSH fill = CreateSolidBrush(gateEnabled_ ? kAccent : kControl);
        HPEN border = CreatePen(PS_SOLID, 1, gateEnabled_ ? kAccent : kBorder);
        HGDIOBJ oldBrush = SelectObject(dc, fill);
        HGDIOBJ oldPen = SelectObject(dc, border);
        RoundRect(dc, box.left, box.top, box.right, box.bottom, Scale(5), Scale(5));
        if (gateEnabled_) {
            HPEN check = CreatePen(PS_SOLID, Scale(2), kBackground);
            SelectObject(dc, check);
            MoveToEx(dc, box.left + Scale(4), box.top + Scale(8), nullptr);
            LineTo(dc, box.left + Scale(7), box.top + Scale(11));
            LineTo(dc, box.left + Scale(12), box.top + Scale(5));
            SelectObject(dc, border);
            DeleteObject(check);
        }
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(fill);
        DeleteObject(border);
        RECT text = item.rcItem;
        text.left += Scale(27);
        SetTextColor(dc, kText);
        DrawTextW(dc, L"轻量噪声门", -1, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        if (item.itemState & ODS_FOCUS) {
            RECT focus = item.rcItem;
            InflateRect(&focus, -Scale(2), -Scale(2));
            DrawFocusRect(dc, &focus);
        }
        return;
    }

    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool hot = (item.itemState & ODS_HOTLIGHT) != 0;
    const COLORREF fillColor = disabled ? kControl : running_ ? (hot ? RGB(35, 41, 37) : kControl) : pressed ? RGB(105, 198, 145) : hot ? RGB(143, 234, 181) : kAccent;
    const COLORREF textColor = disabled ? kMuted : running_ ? kText : kBackground;
    HBRUSH fill = CreateSolidBrush(fillColor);
    HPEN border = CreatePen(PS_SOLID, 1, running_ ? kBorder : fillColor);
    HGDIOBJ oldBrush = SelectObject(dc, fill);
    HGDIOBJ oldPen = SelectObject(dc, border);
    RoundRect(dc, item.rcItem.left, item.rcItem.top, item.rcItem.right, item.rcItem.bottom, Scale(8), Scale(8));
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(fill);
    DeleteObject(border);

    wchar_t text[64];
    GetWindowTextW(item.hwndItem, text, 64);
    RECT textRectangle = item.rcItem;
    SetTextColor(dc, textColor);
    DrawTextW(dc, text, -1, &textRectangle, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (item.itemState & ODS_FOCUS) {
        RECT focus = item.rcItem;
        InflateRect(&focus, -Scale(4), -Scale(4));
        DrawFocusRect(dc, &focus);
    }
}

int AppWindow::Scale(int value) const {
    return MulDiv(value, static_cast<int>(dpi_), 96);
}
