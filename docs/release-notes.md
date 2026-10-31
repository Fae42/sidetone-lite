# Sidetone Lite v1.0.0

## 中文

一个轻量、原生、低占用的 Windows 麦克风耳返工具。选择麦克风，即可在耳机中实时听到自己的声音。

**小、快、省，靠实测说话：** 原生单文件 exe 约 284 KiB；20 分钟监听平均约 0.35% 单核 CPU；内存约 31 MiB，线程和句柄无持续增长。没有 Electron、浏览器或后台服务。

主要功能：

- 输入设备可选，WASAPI 共享音频与低周期尝试
- 0–800% 音量、平滑增益和软限幅
- 可选噪声门与上次麦克风记忆
- 设备变化提示和中英文 README

下载 **SidetoneLite.exe** 后直接运行，无需安装。建议使用有线耳机。量化数据来自本机抽测，实际结果随硬件和驱动而异。

## English

A lightweight, native, low-overhead microphone sidetone tool for Windows. Select a microphone and hear yourself in real time through your headphones.

**Small, fast, and measurably lightweight:** a single native executable of about 284 KiB; approximately 0.35% of one CPU core over a 20-minute monitoring run; about 31 MiB of memory, with no sustained growth in threads or handles. No Electron, browser, or background service.

Highlights:

- Selectable input, shared WASAPI audio, and low-period attempts
- Adjustable 0–800% volume with smooth gain changes and soft limiting
- Optional noise gate and last-used microphone persistence
- Device-change notifications and bilingual Chinese/English README files

Download **SidetoneLite.exe** and run it directly; no installation required. Wired headphones are recommended. Measurements come from local testing and vary by hardware and driver.
