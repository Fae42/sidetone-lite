# Jianting 项目结构调研:GitHub 成熟项目对照

日期:2026-11-01。方法:只读调研;仅引用各官方 GitHub 仓库中的 README、构建文档、CI 配置、LICENSE 等一手来源;star 数取自 GitHub API 当日数值。未修改本文件以外的任何文件。

## 1. Jianting 现状盘点

已有的(不需要大改):

- `src/`:5 个源码模块(`main` / `app_window` / `audio_engine` / `dsp` / `settings`)+ 资源(`.rc` / `.ico` / `.png` / `.manifest`),结构扁平清晰,适合小工具。
- `tests/tests.cpp`:自写测试框架,5 个用例(DSP、环形缓冲、噪声门、INI 设置)。
- `build.ps1` / `test.ps1`:MinGW-w64 g++(C++20)单脚本构建/测试,静态链接出单个 `dist/Jianting.exe`。

缺失的(本次调研的重点):

- 无 README、无 LICENSE、无 `.github/`(即无 CI、无 issue/PR 模板)、无截图、无发布渠道(Releases)、无贡献说明、无 CHANGELOG。

## 2. 参考项目的选择

| 项目 | Star | 类型 | 选中理由 |
|---|---|---|---|
| [EarTrumpet](https://github.com/File-New-Project/EarTrumpet) | ~11.5k | Windows 原生音频小工具(应用) | 与 Jianting 同类:系统托盘音频工具,README 面向最终用户,分发/系统支持/隐私声明做法最值得抄 |
| [RtAudio](https://github.com/thestk/rtaudio) | ~1.8k | C++ 实时音频 I/O 库 | 工程极简(单 .cpp/.h 可直接复制),CI 含 MinGW 构建,与 Jianting 的 g++ 技术栈一致 |
| [miniaudio](https://github.com/mackron/miniaudio) | ~7.4k | 单文件 C 音频库 | README 是现代小项目的良好范本;CI 同时覆盖 MSVC 与 MinGW(UCRT64) |

对比项(明确**不**照搬):

- [PortAudio](https://github.com/PortAudio/portaudio)(~2.2k★):[README](https://github.com/PortAudio/portaudio/blob/master/README.md) 偏旧——正文主要是指向外部网站 portaudio.com 的文档/邮件列表链接,无功能列表、无截图、无下载/安装段。它证明"成熟"不等于"README 范本",不宜模仿其 README。
- [OBS Studio](https://github.com/obsproject/obs-studio)(~78.7k★,GPL-2.0,[COPYING](https://github.com/obsproject/obs-studio/blob/master/COPYING)):[README.rst](https://github.com/obsproject/obs-studio/blob/master/README.rst) 只放徽章+快速链接,构建说明放 wiki;配套 CONTRIBUTING/CODESTYLE/COC 等多份治理文档。这是大型多平台工程的做法,对单脚本小工具是过度设计,只借鉴"构建说明单独成文、README 链过去"这一点。

## 3. README:三个范本的段落对照与借鉴结论

| 段落 | [EarTrumpet](https://github.com/File-New-Project/EarTrumpet/blob/master/README.md) | [RtAudio](https://github.com/thestk/rtaudio/blob/master/README.md) | [miniaudio](https://github.com/mackron/miniaudio/blob/master/README.md) |
|---|---|---|---|
| 一句话定位 | ✅(repo 描述+奖项) | ✅ 首句 | ✅ 副标题 |
| 功能列表 | ✅ Features | ✅ Overview 目标列表 | ✅ Features |
| 示例/截图 | ✖(用媒体评价代替) | ✖ | ✅ 可运行示例代码 |
| 下载/安装 | ✅ Install:Store + winget + choco | ✖ | ✖(建议直接加入源码树) |
| 构建 | ✖(放 [COMPILING.md](https://github.com/File-New-Project/EarTrumpet/blob/master/COMPILING.md)) | ✅ Building(4 种构建系统)+ [install.txt](https://github.com/thestk/rtaudio/blob/master/install.txt) | ✅ Building("像普通源文件一样编译") |
| 测试 | ✖ | ✖(CI 内 `make check`) | ✖(CI 内) |
| 支持平台/系统 | ✅ Supported operating systems(逐版本列出 Win10/11) | ✖(写在 Overview 里) | ✅ Supported Platforms + Backends |
| 已知限制/FAQ | ✖ | ✅ FAQ(单声道问题) | ✖(ABI 兼容性提醒放 Building) |
| 文档链接 | ✅ Documentation(技术细节/编译/贡献/隐私/许可/CHANGELOG) | ✅ Further Reading | ✅ Documentation |
| 隐私声明 | ✅ [PRIVACY.md](https://github.com/File-New-Project/EarTrumpet/blob/master/PRIVACY.md) | ✖ | ✖ |
| 贡献说明 | ✅ 链到 [CONTRIBUTING.md](https://github.com/File-New-Project/EarTrumpet/blob/master/CONTRIBUTING.md) | ✖ | ✖ |
| 许可证 | ✅ 链到 [LICENSE](https://github.com/File-New-Project/EarTrumpet/blob/master/LICENSE)(MIT+排除条款) | ✅ Legal 段+链到 [LICENSE](https://github.com/thestk/rtaudio/blob/master/LICENSE) | ✅ License 段(公有领域/MIT-0 双选,[LICENSE](https://github.com/mackron/miniaudio/blob/master/LICENSE)) |

三个范本的共同骨架:**一句话定位 → 功能 → 安装或构建 → 文档链接 → 平台/限制 → 许可证**。

关于截图:三个 README 都不以截图为核心(EarTrumpet 甚至用媒体评价代替),但它们多是"常驻后台"类工具;Jianting 有可见 GUI 窗口,放 1–2 张主界面截图(README 顶部或 Features 旁)是最有效的第一印象,成本低。

**给 Jianting 的 README 段落清单(全部必要,不多不少):**

1. 项目名 + 一句话定位("Windows 麦克风监听小工具:选择输入设备直接监听,可调音量、带噪声门")。
2. 截图 1–2 张。
3. 功能列表(设备选择/音量/噪声门/设备记忆/开机行为等,与 `app_window.h` 的 UI 能力一一对应)。
4. 下载:GitHub Releases 段(仿 EarTrumpet 的 Install 段,给出单个 exe)。
5. 从源码构建:前置条件(w64devkit 或任意 MinGW-w64 的 g++)+ `./build.ps1`;与 `test.ps1` 一并说明如何跑测试(仿 miniaudio"没有依赖、直接编译"的极简写法)。
6. 系统要求/已知限制:仿 EarTrumpet 逐版本列 Windows 支持;明确已知限制(如独占模式设备、回声问题——监听麦克风必遇)。
7. 隐私说明一小段(仿 EarTrumpet PRIVACY.md 的思路,但一段话即可):音频仅在本地处理、不联网、不落盘。监听类工具没有这句,用户不敢用。
8. 许可证段 + 链接。
9. 贡献:小工具阶段一句话即可("欢迎 issue/PR"),等有外部贡献者再扩成 CONTRIBUTING.md(仿 EarTrumpet 的"issue 先行 + 指定 dev 分支"流程,但不要现在就建)。

## 4. LICENSE

- RtAudio:[MIT 变体](https://github.com/thestk/rtaudio/blob/master/LICENSE)(附加非约束性的"欢迎回馈修改"请求,因此 GitHub 识别为 Other)。
- PortAudio:[MIT 变体](https://github.com/PortAudio/portaudio/blob/master/LICENSE.txt)(同样的非约束性请求)。
- miniaudio:[公有领域 / MIT No Attribution 双许可自选](https://github.com/mackron/miniaudio/blob/master/LICENSE)。
- EarTrumpet:[MIT + 指定实体排除条款](https://github.com/File-New-Project/EarTrumpet/blob/master/LICENSE)(因此 GitHub 识别为 NOASSERTION)。

**结论:Jianting 用标准 MIT 全文即可**——不要加自定义排除/附加条款,否则像上述三个项目一样被 GitHub 判为 "Other/NOASSERTION",仓库页面不显示许可证徽标。LICENSE 放根目录,README 末尾一段链接过去。

## 5. CI 配置

| 项目 | Workflow(均为 push/PR 触发) | 可借鉴度 |
|---|---|---|
| RtAudio | [单个 `ci.yml`](https://github.com/thestk/rtaudio/blob/master/.github/workflows/ci.yml):tarball `distcheck` + 平台矩阵(Linux 按音频后端、macOS、**MinGW 交叉编译 i686/x86_64 × dsound/asio/wasapi**) | ★★★ 矩阵按"后端×架构"展开的思路;证明 MinGW 构建 WASAPI 可以进 CI |
| miniaudio | [单个 `ci.yml`](https://github.com/mackron/miniaudio/blob/master/.github/workflows/ci.yml):Windows 上 **MSVC 与 MinGW(UCRT64,`msys2/setup-msys2`)** 双编译器矩阵、`-DMINIAUDIO_WARNINGS_AS_ERRORS=ON`、构建测试+示例;另覆盖 Linux/macOS/BSD/Android | ★★★ 与 Jianting 最相关:`windows-latest + MSYS2 MinGW g++` 正好能跑 `build.ps1`/`test.ps1`;警告即错误值得抄 |
| PortAudio | [6 个 workflow](https://github.com/PortAudio/portaudio/tree/master/.github/workflows)(MSBuild/autotools/autotools_msys2/cmake/compare_def_files/whitelint),按构建系统拆分 | ★☆ 对单脚本项目过重;仅 `autotools_msys2.yml` 证明 MSYS2 方案可行 |
| EarTrumpet | [main.yml](https://github.com/File-New-Project/EarTrumpet/blob/master/.github/workflows/main.yml)(约 23KB,构建+签名+向 dev 通道自动发布)+ 翻译/赞助辅助 workflow | ★ 自动发布思路好(README 里"dev 通道每次提交自动出新版"),但 Store 签名发布链路对小工具是超前配置 |

**给 Jianting 的最小 CI(一个文件够):** `.github/workflows/ci.yml`,`on: [push, pull_request]`,一个 `windows-latest` job,装 MSYS2 MinGW g++,依次跑 `./build.ps1` 和 `./test.ps1`,用 `actions/upload-artifact` 传出 `dist/Jianting.exe`;打 `v*` tag 时额外跑一次并附到 GitHub Releases。不需要矩阵、不需要多构建系统、不需要 Linux/macOS。

## 6. 明确不建议照搬的东西

- OBS 的 CMake presets/多模块仓库结构和 CONTRIBUTING/CODESTYLE/COC 治理文档体系;
- EarTrumpet 的 Crowdin 翻译、赞助、签名发布链路;
- PortAudio/RtAudio 的多构建系统并存(autotools+CMake+meson)——Jianting 保留 `build.ps1` 即可,引入 CMake 属于可选项而非必要项;
- 库类项目(RtAudio/miniaudio)的"单文件分发"定位描述——Jianting 是最终用户应用,README 应学 EarTrumpet 的"Install 优先",而不是库的"嵌入你项目"。

## 7. 落地顺序建议(按性价比)

1. LICENSE(标准 MIT,5 分钟);
2. README(按第 3 节 9 段清单);
3. 截图 2 张(开发机截屏即可);
4. GitHub Releases + 打 tag(把 `dist/Jianting.exe` 发出去);
5. `.github/workflows/ci.yml`(按第 5 节最小 CI);
6. CHANGELOG.md 与 CONTRIBUTING.md 等有真实需要再补。

## 来源清单

- EarTrumpet:仓库 https://github.com/File-New-Project/EarTrumpet ｜ README https://github.com/File-New-Project/EarTrumpet/blob/master/README.md ｜ COMPILING.md https://github.com/File-New-Project/EarTrumpet/blob/master/COMPILING.md ｜ CONTRIBUTING.md https://github.com/File-New-Project/EarTrumpet/blob/master/CONTRIBUTING.md ｜ LICENSE https://github.com/File-New-Project/EarTrumpet/blob/master/LICENSE ｜ CI https://github.com/File-New-Project/EarTrumpet/blob/master/.github/workflows/main.yml
- RtAudio:仓库 https://github.com/thestk/rtaudio ｜ README https://github.com/thestk/rtaudio/blob/master/README.md ｜ install.txt https://github.com/thestk/rtaudio/blob/master/install.txt ｜ LICENSE https://github.com/thestk/rtaudio/blob/master/LICENSE ｜ CI https://github.com/thestk/rtaudio/blob/master/.github/workflows/ci.yml
- miniaudio:仓库 https://github.com/mackron/miniaudio ｜ README https://github.com/mackron/miniaudio/blob/master/README.md ｜ LICENSE https://github.com/mackron/miniaudio/blob/master/LICENSE ｜ CI https://github.com/mackron/miniaudio/blob/master/.github/workflows/ci.yml
- PortAudio(对比):仓库 https://github.com/PortAudio/portaudio ｜ README https://github.com/PortAudio/portaudio/blob/master/README.md ｜ LICENSE.txt https://github.com/PortAudio/portaudio/blob/master/LICENSE.txt ｜ workflows https://github.com/PortAudio/portaudio/tree/master/.github/workflows
- OBS Studio(对比):仓库 https://github.com/obsproject/obs-studio ｜ README.rst https://github.com/obsproject/obs-studio/blob/master/README.rst ｜ COPYING(GPL-2.0) https://github.com/obsproject/obs-studio/blob/master/COPYING
- star 数与许可证元数据:GitHub API repos 端点(2026-11-01 查询),如 https://api.github.com/repos/File-New-Project/EarTrumpet
