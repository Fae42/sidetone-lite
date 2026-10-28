# Mic Monitor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 构建一个独立、低占用的 Windows 原生麦克风耳返 exe，支持输入选择、音量、启停与可开关噪声门。

**Architecture:** Win32 界面与 WASAPI 音频引擎分离，界面只通过线程安全调用调整音频状态。采集与渲染使用共享、事件驱动流，在专用音频线程中完成格式转换、重采样、缓冲、音量及噪声门处理。

**Tech Stack:** C++20、Win32/GDI、WASAPI、Windows MMDevice API、便携 MinGW-w64、PowerShell 构建脚本。

**Spec:** `docs/superpowers/specs/2026-10-27-mic-monitor-design.md`

## Global Constraints

- 目标为 Windows 10/11 x64；输出文件为 `dist/Jianting.exe`。
- 只用操作系统音频与界面 API；最终 exe 静态链接 GCC 运行库，不要求安装浏览器、.NET、Visual Studio 或第三方音频运行库。
- WASAPI 使用共享模式，输出使用启动时的默认多媒体端点，不使用独占模式。
- 噪声门默认关闭；音量范围 0–800%，100% 为原始增益。
- 不联网、不保存录音、不安装服务、不设置自启动；关闭窗口后音频线程与进程都退出。
- 项目没有既有测试，不新增测试脚手架或测试文件；每个任务使用构建结果和明确的运行检查验收。
- 当前目录不是 Git 仓库，不初始化仓库、不执行提交。

## Review Focus

- 麦克风与输出采样率或声道数不同：Task 2 的转换与漂移缓冲必须处理，Task 4 用所有可用输入逐一检查。
- 监听中断开所选麦克风：Task 3 必须收到设备通知并显示可恢复错误，Task 4 实际拔插检查。
- 监听中切换 Windows 默认输出：Task 3 不得继续向旧端点无声播放，Task 4 检查提示与恢复。
- 轻声说话及句尾遇到噪声门：Task 4 分别检查开关状态，确认无硬切、咔哒声或明显吞字。
- 独立音频时钟长期漂移：Task 2 限制缓冲并自适应修正，Task 4 连续运行检查爆音、延迟增长与内存增长。

---

### Task 1: 独立原生构建骨架

**Files:**
- Create: `build.ps1`
- Create: `src/main.cpp`

**Interfaces:**
- Produces: 构建命令 `powershell -NoProfile -ExecutionPolicy Bypass -File ./build.ps1`，输出 `dist/Jianting.exe`。
- Produces: `wWinMain` 程序入口，后续任务保留该入口并接入应用窗口。

- [ ] **Step 1: 准备项目内便携编译器**

从 `skeeto/w64devkit` 的 GitHub 正式 release 获取 x64 便携包并解压到 `tools/w64devkit`。不改系统 PATH、不安装系统组件；确认 `tools/w64devkit/bin/g++.exe --version` 能运行。

- [ ] **Step 2: 编写可复用的构建脚本**

`build.ps1` 优先使用 PATH 中的 `g++`，否则使用上述便携编译器。使用 C++20、`-Os`、函数/数据分段与链接器垃圾回收、去除调试符号、Unicode、`-mwindows`、静态 GCC 运行库及必要 Windows 系统库；不使用 UPX 等压缩壳。编译错误必须令脚本返回非零，成功时创建 `dist/Jianting.exe`。

- [ ] **Step 3: 创建最小原生窗口**

在 `src/main.cpp` 创建带标准关闭行为的 Unicode Win32 窗口，进程退出码为消息循环结果。此步骤只验证工具链和窗口生命周期，不包含音频占位逻辑。

- [ ] **Step 4: 验证干净构建与启动**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File ./build.ps1`
Expected: 返回 0 且生成 `dist/Jianting.exe`；双击后出现窗口，关闭后 `Get-Process Jianting` 无残留进程。

### Task 2: WASAPI 耳返引擎与最小可用界面

**Files:**
- Create: `src/audio_engine.h`
- Create: `src/audio_engine.cpp`
- Create: `src/dsp.h`
- Create: `src/dsp.cpp`
- Create: `src/app_window.h`
- Create: `src/app_window.cpp`
- Modify: `src/main.cpp`
- Modify: `build.ps1`

**Interfaces:**
- Produces: `InputDevice { std::wstring id; std::wstring name; }`。
- Produces: `enum class AudioState { Stopped, Running, Error }` 与 `using AudioEventCallback = void (*)(AudioState state, const std::wstring& message, void* context)`。
- Produces: `AudioEngine::ListInputs(std::vector<InputDevice>& devices, std::wstring& error)`。
- Produces: `AudioEngine::Start(const std::wstring& inputId, AudioEventCallback callback, void* context, std::wstring& error)` 与 `AudioEngine::Stop() noexcept`；`Stop()` 返回后不再触发回调。
- Produces: `AudioEngine::SetVolume(float normalized) noexcept` 与 `AudioEngine::SetNoiseGateEnabled(bool enabled) noexcept`，可从界面线程调用。
- Consumes: `AppWindow` 拥有 `AudioEngine`，音频回调只向窗口 `PostMessage`，不直接操作控件。

- [ ] **Step 1: 实现设备枚举与共享流初始化**

枚举 `eCapture` 的活动端点并保存稳定设备 ID 与友好名称。启动时在音频线程重新激活输入端点和默认 `eMultimedia` 输出端点，创建事件驱动的采集、渲染客户端；使用默认引擎周期与禁用持久化的流标志，失败路径释放全部 COM 对象和事件句柄。

- [ ] **Step 2: 实现有界音频缓冲与格式处理**

以输出采样率和声道数作为内部格式。将常见 WAVEFORMATEX/WAVEFORMATEXTENSIBLE PCM 或 float 输入转换为 float，完成单声道/立体声映射及线性重采样，渲染前再转换到实际输出格式。环形缓冲预填充约 5 ms，容量不超过 250 ms，欠载后重新预填；根据缓冲水位微调重采样比率，防止不同设备时钟造成持续欠载或溢出。静音包与捕获断点必须产生确定行为。

- [ ] **Step 3: 实现事件驱动音频循环**

使用停止、采集、渲染事件等待，不做忙轮询。渲染前预填静音，按 padding 写入可用帧；无数据时输出静音。停止时先通知线程，再停止流并 join，避免退出后回调或句柄泄漏。为音频线程使用 MMCSS Audio/Pro Audio 配置并在退出时还原。

- [ ] **Step 4: 接入音量与轻量噪声门**

音量使用原子值并在 float 样本上应用，输出前限制到合法范围。噪声门默认关闭，使用约 -45 dBFS 阈值、短进入和平滑释放；开关切换不得产生硬切。不得加入持续降噪、网络或磁盘音频处理。

- [ ] **Step 5: 用最小 Win32 控件完成端到端接线**

提供输入下拉框、0–800 音量滑条、噪声门复选框、开始/停止按钮和状态文本。运行时禁用输入选择，停止后可更改；所有启动错误显示为中文状态，窗口关闭必须调用 `Stop()`。

- [ ] **Step 6: 构建并执行第一次真实耳返检查**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File ./build.ps1`
Expected: 编译无错误。戴耳机选择真实麦克风后能开始和停止，音量 0% 静音、100% 不额外放大，噪声门可即时切换；没有设备独占错误，退出无残留进程。

### Task 3: 轻量质感界面与设备变化恢复

**Files:**
- Modify: `src/app_window.h`
- Modify: `src/app_window.cpp`
- Modify: `src/audio_engine.h`
- Modify: `src/audio_engine.cpp`
- Create: `src/resource.h`
- Create: `src/Jianting.rc`
- Modify: `build.ps1`

**Interfaces:**
- Consumes: Task 2 的 `AudioEngine` 公共接口保持不变。
- Produces: 音频/设备通知通过窗口消息进入 UI 线程；COM 通知回调不持有窗口锁或直接调用控件。
- Produces: 资源脚本中的中文产品名与版本信息，最终仍只生成一个 exe。

- [ ] **Step 1: 完成紧凑深色视觉**

将窗口控制为约 360×260 像素的客户区，使用深色哑光背景、清晰层级、统一间距、中文界面字体和单一运行强调色。为标题栏启用系统深色模式；控件自绘或主题化必须覆盖正常、悬停、禁用和焦点状态，不使用动画或虚假性能指标。

- [ ] **Step 2: 完善交互状态与反馈**

运行状态、停止状态、错误状态在颜色和文案上可区分；音量显示实际百分比，启停期间避免重复点击造成多个音频线程。提供简短的耳机防啸叫提示，所有布局在 100% 与常见高 DPI 缩放下不裁切。

- [ ] **Step 3: 处理端点与默认输出变化**

注册 `IMMNotificationClient` 并把通知编组到窗口线程。设备列表变化时按设备 ID 保留选择；运行中的输入被移除时停止并显示错误；默认输出变化时停止旧流并明确提示重新开始。窗口销毁时注销通知。

- [ ] **Step 4: 添加原生程序资源并验证所有状态**

在资源脚本加入应用版本信息与简洁图标，构建后检查 exe 属性和窗口图标。逐项检查控件禁用、启动失败、停止、重开、拔插输入和切换默认输出，确认没有卡死、静默失效或多个实例的音频线程残留。

### Task 4: 发布构建与实际场景验收

**Files:**
- Modify only if verification exposes a defect: `src/*.cpp`, `src/*.h`, `build.ps1`
- Verify: `dist/Jianting.exe`

**Interfaces:**
- Consumes: 前面任务的全部功能。
- Produces: 用户可直接运行的 `dist/Jianting.exe`，以及实际验证结果；不把便携工具链作为交付依赖。

- [ ] **Step 1: 重新构建并检查交付物**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File ./build.ps1`
Expected: 返回 0；exe 可从不含源码的工作目录启动，不出现缺少 GCC/MinGW DLL 的错误。

- [ ] **Step 2: 验证每项功能与设备变化**

戴有线耳机检查所有可用输入、启停、0/50/100/200% 音量、噪声门开关、输入拔插、默认输出切换和重复启动。确认界面状态与实际音频一致，任何设备错误都可恢复。

- [ ] **Step 3: 测量监听资源占用**

用 `Get-Process Jianting` 记录静止与监听状态的工作集，并在固定间隔采样累计 CPU 计算监听期间平均占用。记录机器、测量时长和结果，不用单次瞬时值声称性能。

- [ ] **Step 4: 连续运行稳定性检查**

连续监听至少 20 分钟，期间说话并开关噪声门。检查缓冲不持续增长、没有周期性爆音、线程/句柄/内存不持续增长；结束后进程完全退出。

- [ ] **Step 5: 由用户完成最终听感确认**

让用户在实际游戏用的麦克风与耳机上确认延迟、底噪、音量和游戏内表现。若缺少可用音频端点或无法实际听测，明确列出未验证项，不把编译成功当作完整验收。
