# FastMode v0.4.0

FastMode 的正式发布版本，也是首个以 MIT 许可证发布的版本。

## 本版新增

- 原生模块完成 clean-room 重构：进程倍速核心改为独立实现，清除全部 Cheat Engine 相关代码与符号，不再依赖任何 CE 授权。
- 时间缩放改为拦截 `GetTickCount`、`GetTickCount64`、`QueryPerformanceCounter`（`RtlQueryPerformanceCounter`）与 `timeGetTime`，倍率切换时对时间基线重新锚定，保证过渡平滑。
- DLL 导出接口更新为 `FmInitThread` / `FmSetSpeedThread`，主程序注入流程同步适配。
- 项目正式以 MIT License 发布，新增 `LICENSE` 与 `THIRD_PARTY_NOTICES.md`，发布包随附。
- 新增进程内冒烟测试 `tests/timing_smoke`，验证倍速下的时间缩放比例（2 倍速实测约 1.97x）。

## 保留功能

- 原生 WPF 桌面界面，不依赖本地 Web 服务。
- 仅枚举具有可见顶层窗口的目标应用。
- 支持预设倍速和自定义倍率。
- 支持默认 `LB + RB` 手柄组合键及可视化改键。
- 支持默认 `Ctrl + F` 全局键盘快捷键及可视化改键。
- 简体中文与英文运行时切换。
- 进程倍速启用时默认同步处理音频。
- Spatial Audio 路径集成 SoundTouch 保音调时间伸缩。
- 支持 Windows `PerMonitorV2` 多显示器高 DPI。

## 实现说明

进程倍速核心为独立实现：通过 MinHook 拦截进程的四个主要计时来源并按倍率缩放流逝时间；原理基于公开的 Windows API 文档知识，源码不包含也不引用 Cheat Engine 的任何代码。音频同步、SoundTouch 处理与桌面界面均为 FastMode 自研。

## 当前限制

- 当前仅支持 Windows x64 目标进程。
- 受保护进程和反作弊环境可能无法附加。
- 不同程序的计时与音频实现不同，需要逐个验证兼容性。
- 当前程序未进行 Windows 代码签名。
