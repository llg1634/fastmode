# FastMode v0.3.0 Preview

FastMode 的第二个公开预览版本准备稿。

## 本版新增

- 新增默认 `Ctrl + F` 全局键盘快捷键。
- 键盘快捷键与手柄快捷键复用同一套目标附加、倍速切换和悬浮窗同步流程。
- 键盘快捷键不会拦截原有输入，按键释放后才可再次触发。
- 设置页新增可视化键盘按键选择，不需要手动输入。
- 键盘组合必须包含 `Ctrl`、`Shift` 或 `Alt`，并选择一个字母、数字或功能键。
- 新增简体中文与英文运行时切换，程序每次启动默认使用中文。
- 修复白色背景下悬浮窗开关状态不清晰的问题，开关药丸增加绿色描边。

## 保留功能

- 原生 WPF 桌面界面，不依赖本地 Web 服务。
- 仅枚举具有可见顶层窗口的目标应用。
- 支持预设倍速和自定义倍率。
- 支持默认 `LB + RB` 手柄组合键及可视化改键。
- 进程倍速启用时默认同步处理音频。
- Spatial Audio 路径集成 SoundTouch 保音调时间伸缩。
- 支持 Windows `PerMonitorV2` 多显示器高 DPI。

## Cheat Engine 参考实现

进程倍速核心基于并参考 Cheat Engine Speedhack 的实现思路与接口行为：

- GitHub：[cheat-engine/cheat-engine](https://github.com/cheat-engine/cheat-engine)
- 官网：[Cheat Engine](https://cheatengine.org/)

FastMode 不是 Cheat Engine 官方项目。WPF 界面、快捷键控制、音频同步和 SoundTouch 处理属于 FastMode 的集成与扩展。

## 当前限制

- 当前仅支持 Windows x64 目标进程。
- 受保护进程和反作弊环境可能无法附加。
- 不同程序的计时与音频实现不同，需要逐个验证兼容性。
- 当前程序未进行 Windows 代码签名。
