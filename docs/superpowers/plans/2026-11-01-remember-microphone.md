# Remember Microphone Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 应用重启后自动恢复上次成功监听的麦克风，设备缺失时安全回退。

**Architecture:** 在现有 `AppWindow` 中增加轻量 INI 读写辅助函数和 `rememberedInputId_` 成员。配置只在成功启动监听后写入，首次枚举设备时按稳定 ID 恢复，不改动音频引擎。

**Tech Stack:** C++20、Win32 INI API、现有 MinGW-w64 构建脚本。

**Spec:** `docs/superpowers/specs/2026-11-01-remember-microphone-design.md`

## Global Constraints

- 配置文件为 exe 同目录的 `Jianting.ini`，节为 `[Audio]`，键为 `LastInputId`。
- 只在成功开始监听后保存，不保存音量、噪声门或其他设置。
- 当前下拉选择优先；只有没有当前选择时才使用 INI。
- INI 读写失败不得阻断监听，不新增第三方依赖或测试文件。

## Review Focus

- 首次启动且 INI 不存在：使用首个活动设备且不报错。
- INI 中设备不存在：回退到首个活动设备，保留配置供设备重新出现。
- INI 目录不可写：监听仍可使用，只是不保存选择。
- 用户已主动改变选择：刷新时不得被旧 INI 覆盖。

---

### Task 1: 保存并恢复上次麦克风

**Files:**
- Modify: `src/app_window.h`
- Modify: `src/app_window.cpp`

**Interfaces:**
- Produces: 匿名命名空间辅助函数 `std::wstring GetSettingsPath()`、`std::wstring LoadRememberedInputId()`、`void SaveRememberedInputId(const std::wstring& id)`。
- Produces: `AppWindow::rememberedInputId_`，仅由 UI 线程访问。

- [ ] **Step 1: 实现 INI 路径与读写**

使用 exe 模块路径构造同目录 `Jianting.ini`，通过 `GetPrivateProfileStringW` 读取 `[Audio].LastInputId`，通过 `WritePrivateProfileStringW` 写入；空 ID 不写入，失败不弹窗。

- [ ] **Step 2: 接入首次恢复与成功启动保存**

在 `OnCreate` 最终 `RefreshDevices()` 前加载 ID；`RefreshDevices()` 先保留当前选择，无有效选择时回退到 INI。`engine_.Start()` 成功后更新成员并保存，失败时不改写配置。

- [ ] **Step 3: 构建与真实重启验证**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File ./build.ps1`
Expected: 返回 0。选择 HyperX 并成功监听，关闭后重新打开，组合框自动选中 HyperX；启动/停止正常，INI 位于 exe 旁且只包含指定设备 ID。

- [ ] **Step 4: 提交 Git**

Run: `git add src/app_window.h src/app_window.cpp docs/superpowers && git commit -m "feat: remember last microphone"`
Expected: 创建提交，`git status --short` 无源码未提交变更。
