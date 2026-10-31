# Sidetone Lite

一个轻量、原生、低占用的 Windows 麦克风耳返工具。选择麦克风，即可在耳机中实时听到自己的声音。

![Sidetone Lite 主界面](docs/images/window.png)

## 功能

- 输入设备可选，输出跟随 Windows 默认设备
- WASAPI 共享音频，优先尝试驱动支持的最低周期，失败时自动回退
- 音量 0–800% 可调，带平滑增益与软限幅
- 可开关的轻量噪声门，默认关闭
- 记住上次成功使用的麦克风
- 设备断开、默认输出变化时给出明确提示
- 单文件原生 exe，约 282 KiB，无浏览器或后台服务

## 下载

从 [Releases](https://github.com/Fae42/sidetone-lite/releases/latest) 下载 `SidetoneLite.exe`，直接运行即可，无需安装。

**请使用耳机，尤其是有线耳机。** 音箱外放可能产生啸叫；蓝牙耳机本身也会增加延迟。

## 从源码构建

要求：

- Windows 10/11 x64
- MinGW-w64 g++（支持 C++20）
- Windows PowerShell 5.1 或 PowerShell 7+

如果 PATH 中没有 `g++`，脚本会尝试使用项目内的 `tools/w64devkit/bin/g++.exe`。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ./build.ps1
```

输出：`dist/Jianting.exe`。

## 测试

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ./test.ps1
```

测试在控制台中构建并运行，不打开监听窗口、不访问真实麦克风。覆盖：

- INI 配置读写与空值
- 高倍率重采样和跨包边界
- 环形缓冲溢出与静音填充
- 增益限幅
- 噪声门状态转换

CI 在每次 push 和 pull request 时执行同样的构建与测试，并上传 Windows x64 可执行文件。

## 使用说明与限制

- 启动后选择麦克风，点击“开始监听”；运行时先停止才能切换输入。
- 100% 是原始增益；超过 100% 会同时放大底噪，过高增益可能产生压缩感。
- 实际端到端延迟还取决于 Windows 音频引擎、驱动和耳机，不等于界面显示的周期。
- 低周期是否生效由输出驱动决定；状态栏显示实际接受的周期。
- `Jianting.ini` 保存在 exe 旁，仅记录设备 ID。目录不可写时只是不记忆，不影响监听。
- 不支持录音、持续 AI 降噪、虚拟声卡、独占模式或多输出路由。

## 隐私

音频只在本地内存中处理，不联网、不写入录音文件，也不设置开机自启动。配置文件只包含上次成功使用的麦克风 ID。

## 项目结构

```text
src/                 原生界面、WASAPI、DSP 与 INI 设置
tests/               无第三方框架的自动化测试
docs/                设计说明与项目结构调研
build.ps1            应用构建脚本
test.ps1             测试构建与运行脚本
.github/workflows/   Windows CI
```

## 贡献

欢迎提交 issue 或 pull request。开始修改前请阅读 [贡献说明](CONTRIBUTING.md)，并确保 `./build.ps1` 和 `./test.ps1` 均通过。

## 许可证

[MIT](LICENSE)
