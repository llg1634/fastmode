FastMode is a lightweight native process speed controller for Windows x64 applications, featuring visible-window target selection, preset and custom multipliers, synchronized audio acceleration, XInput controller hotkeys, and Per-Monitor V2 DPI support. Its process-speed core is based on and references the Cheat Engine Speedhack implementation. For the full English documentation, see [README_EN.md](./README_EN.md).

<div align="center">
  <img src="./assets/FastMode.png" width="128" alt="FastMode 图标">
  <h1>FastMode</h1>
  <p>面向 Windows x64 应用的轻量级进程倍速控制工具。</p>
  <p>原生 WPF 界面 · 手柄快捷键 · 音频同步加速 · Per-Monitor V2 高 DPI</p>
</div>

## 项目简介

FastMode 将进程倍速控制整理为一个独立的 Windows 桌面程序。用户可以从具有可见前台窗口的应用列表中选择目标，设置预设或自定义倍速，并通过界面开关或手柄组合键快速启停。

项目使用原生 WPF 构建，不依赖本地 Web 服务或浏览器端口。界面采用绿色设计语言、自绘圆角窗口壳，并支持在不同 DPI 的显示器之间动态切换和重新渲染。

## Cheat Engine 参考实现

FastMode 的进程倍速核心并非从零设计。原生模块基于并参考了 **Cheat Engine Speedhack** 的实现思路与接口行为，包括对进程计时来源的处理、倍率切换时的时间基线衔接，以及相关 x64 原生注入结构。

主要参考来源：

- GitHub：[cheat-engine/cheat-engine](https://github.com/cheat-engine/cheat-engine)
- 官网：[Cheat Engine](https://cheatengine.org/)

FastMode 在此基础上进行了独立封装与扩展，包括：

- 原生 WPF 桌面界面
- 仅显示具有可见顶层窗口的目标应用
- XInput 手柄状态检测和组合键切换
- 音频路径挂钩与同步倍率处理
- Spatial Audio 影子缓冲处理
- SoundTouch 保持音调的时间伸缩
- Windows `PerMonitorV2` 多显示器高 DPI 适配

**FastMode 不是 Cheat Engine 官方项目，也不受 Cheat Engine 官方维护或背书。** 音频同步、SoundTouch 处理和桌面控制界面属于 FastMode 侧的集成与扩展，不应理解为 Cheat Engine 官方功能说明。

## 当前状态

| 项目 | 状态 |
|---|---|
| 当前版本 | `v0.2.0 Preview` |
| 操作系统 | Windows 10 / Windows 11 |
| 架构 | x64 |
| 界面语言 | 简体中文 |
| 主界面 | 原生 WPF |
| DPI | Per-Monitor V2 |
| 公开下载 | 尚未发布 |

## 主要功能

- 枚举具有可见顶层窗口的应用，并显示应用图标、窗口标题、进程名、PID 和架构。
- 附加或断开目标进程。
- 提供常用倍速预设和自定义倍率输入。
- 使用界面开关启用或关闭倍速。
- 默认使用 `LB + RB` 手柄组合键切换倍速状态。
- 支持在设置页直接选择新的手柄按键组合。
- 支持主窗口置顶和可选的小型置顶状态悬浮窗。
- 启用倍速时默认同步处理目标音频，不提供单独的音频开关。
- 支持跨不同缩放比例的显示器移动，保持文字、图标和圆角边框清晰。

## 界面预览

![FastMode 主界面](./assets/screenshot-main.png)

## 快速开始

1. 解压完整发布包，不要只单独复制 `FastMode.exe`。
2. 运行 `FastMode.exe`。
3. 在“目标进程”列表中选择需要控制的应用。
4. 点击“附加”。
5. 选择预设倍速，或输入自定义倍率后点击“应用”。
6. 打开“启用”开关，或按下默认手柄快捷键 `LB + RB`。
7. 不再使用时先关闭倍速或断开目标，然后退出 FastMode。

如果目标程序以管理员权限运行，FastMode 通常也需要使用相同或更高权限启动。

## 音频同步说明

进程倍速启用后，音频默认跟随同一个倍率，不需要额外设置。

当前原生模块覆盖或探测的音频路径包括 XAudio2、DirectSound、waveOut、WASAPI 和 Windows Spatial Audio。Spatial Audio 路径使用扩展缓冲区收集更多源样本，并通过 SoundTouch 执行时间伸缩，以尽量在提高播放速度时保持原始音调。

不同程序使用的音频引擎、样本格式和提交方式并不相同，因此音频同步效果需要按目标程序实际验证。非 Spatial Audio 路径可能采用不同回退方式，不能保证所有程序都具有相同的保音调效果。

## 兼容性与限制

- 当前发布包仅支持 x64 目标进程。
- 受保护进程、反作弊环境或禁止远程线程注入的程序可能无法附加。
- 不使用常见 Windows 计时来源的程序可能无法正确响应倍率。
- 某些程序会在附加前完成音频初始化，晚注入兼容性取决于具体音频路径。
- 当前程序未进行代码签名，Windows 可能显示未知发布者提示。
- 修改目标进程运行状态存在兼容性风险，请先保存目标程序中的重要数据。

## 版本与开发状态

当前审核版本为 `v0.2.0 Preview`，主要功能仍在实际目标程序中持续验证。

- 桌面端以 WPF 原生版本为当前主线。
- 仓库中的 Tauri/Web UI 仅作为早期实现保留。
- x86 目标进程支持尚未纳入当前发布范围。
- 正式公开版本号、分支策略和兼容性列表将在审核后确定。

## 从源码构建

### 桌面程序

需要 .NET 8 SDK：

```powershell
cd wpf\FastMode.Desktop
dotnet build -c Release
```

### 原生 Speedhack 模块

当前构建脚本使用 MinGW-w64 x64 工具链，并静态链接 SoundTouch：

```powershell
powershell -ExecutionPolicy Bypass -File native\speedhack\build.ps1
```

构建后的 `speedhack_x64.dll` 必须与 FastMode 一同发布。

## 目录说明

```text
fastmode/
├─ wpf/FastMode.Desktop/       原生 WPF 桌面程序
├─ native/speedhack/           进程与音频倍速原生模块
├─ src-tauri/resources/        原生 DLL 的发布资源位置
└─ docs/                       项目文档
```

仓库中保留了早期 Tauri/Web UI 代码，但当前日常使用和发布目标均为 WPF 原生版本。

## 第三方项目与致谢

- [Cheat Engine](https://github.com/cheat-engine/cheat-engine)：进程 Speedhack 实现的重要参考来源。
- [Cheat Engine 官网](https://cheatengine.org/)：Cheat Engine 官方网站。
- [SoundTouch](https://www.surina.net/soundtouch/)：音频 tempo、pitch 和 rate 处理。
- [MinHook](https://github.com/TsudaKageyu/minhook)：Windows x86/x64 API Hook 库。

发布包应保留 `licenses` 目录中的第三方许可证文本。

## 参与开发

公开仓库建立后，可以通过 Issue 提交兼容性问题、目标程序音频路径、手柄型号和多显示器 DPI 测试结果。代码修改应尽量附带复现步骤、目标架构和验证结果。

## 许可证

FastMode 自身的最终开源许可证尚未确定，公开发布前需要完成许可证审核。

第三方组件继续遵循其各自许可证。当前使用的 SoundTouch 许可证文本和 MinHook 许可证文本已经放入发布包的 `licenses` 目录。由于原生模块参考并使用了 Cheat Engine Speedhack 的实现结构，正式公开源码和二进制文件前还需要确认并满足对应上游许可要求。

## 免责声明

本项目用于软件兼容性研究、个人测试和可控环境下的运行速度调节。使用者应自行确认目标软件的许可协议、平台规则和适用法律，并自行承担使用风险。
