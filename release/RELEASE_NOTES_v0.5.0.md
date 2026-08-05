# FastMode v0.5.0

FastMode 在 v0.4.0 基础上的兼容性更新版本。

## 本版新增

- 新增通用 WASAPI 保音高处理路径。
- 普通 WASAPI 游戏倍速时使用 SoundTouch 执行 `tempo=speed, pitch=1`，速度变化但音高保持。
- 支持 WASAPI 16/32 位 PCM 或 float 格式；不支持的格式自动回退到原有抽样逻辑。
- 已对《Beast of Reincarnation》实测：2.0x 时 WASAPI 缓冲完成 time-stretch，保音高状态位生效。
- 现有 XAudio2、DirectSound、waveOut、Spatial Audio 路径保持不变。

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

进程倍速核心为独立实现：通过 MinHook 拦截进程的四个主要计时来源并按倍率缩放流逝时间；原理基于公开的 Windows API 文档知识，源码不包含也不引用 Cheat Engine 的任何代码。WASAPI 与 Spatial Audio 的 SoundTouch 处理均为 FastMode 自研。

## 当前限制

- 当前仅支持 Windows x64 目标进程。
- 受保护进程和反作弊环境可能无法附加。
- 不同程序的计时与音频实现不同，需要逐个验证兼容性。
- 当前程序未进行 Windows 代码签名。
