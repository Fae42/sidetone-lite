# 贡献说明

这个小工具优先保持低延迟、低占用和简单可靠，不追求功能数量。

## 修改前

- 用 issue 说明问题、复现步骤和期望行为。
- 小修复可直接提交 pull request；较大的音频链路或界面调整请先讨论。
- 不引入大型框架、后台服务、联网或不必要的依赖。

## 提交前

在 Windows 上运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ./build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File ./test.ps1
```

两项都必须通过。修改 DSP、重采样、缓冲或 INI 逻辑时，请同时补充或调整 `tests/tests.cpp` 中的用例。测试不得依赖真实麦克风、桌面操作或人工听测。

## 代码约定

- 使用现有 C++20、Win32 和 WASAPI 风格，不引入另一套构建系统。
- 保持错误可见、资源可释放、音频线程不忙于轮询。
- 不改变用户已确认的共享模式、低占用和隐私约束。
