简体中文版本请参阅 [README.md](./README.md)。

<div align="center">
  <img src="./assets/FastMode.png" width="128" alt="FastMode icon">
  <h1>FastMode</h1>
  <p><strong>A lightweight process speed controller for Windows x64 applications.</strong></p>
  <p>Native WPF UI · Controller and keyboard shortcuts · Chinese/English switching · Synchronized audio speed</p>
</div>

## Overview

FastMode packages process speed control into a standalone Windows desktop application. It lists applications with visible top-level windows, lets the user select a target, configure a preset or custom multiplier, and toggle acceleration from the UI, a controller shortcut, or a global keyboard shortcut.

The current desktop application is built with native WPF and does not depend on a local web server or browser port. It uses a green visual language, a custom rounded window shell, and Per-Monitor V2 DPI awareness for sharp rendering across displays with different scaling factors. Simplified Chinese is used by default; click the language button at the top of the window to switch between Chinese and English at runtime.

## Cheat Engine Reference Implementation

FastMode's process speed core was not designed from scratch. The native module is based on and references the implementation concepts and interface behavior of **Cheat Engine Speedhack**, including process timing sources, continuous time baselines when changing multipliers, and the related x64 native injection structure.

Primary references:

- GitHub: [cheat-engine/cheat-engine](https://github.com/cheat-engine/cheat-engine)
- Official website: [Cheat Engine](https://cheatengine.org/)

FastMode adds its own application-level integration and extensions, including:

- A native WPF desktop interface
- Target discovery limited to applications with visible top-level windows
- XInput controller detection and combination hotkeys
- Audio-path hooks and synchronized multiplier handling
- Spatial Audio shadow-buffer processing
- Pitch-preserving time stretching with SoundTouch
- Windows `PerMonitorV2` multi-display DPI support

**FastMode is not an official Cheat Engine project and is not maintained, sponsored, or endorsed by the Cheat Engine project.** Audio synchronization, SoundTouch processing, and the desktop control interface are FastMode-side integrations and extensions, not descriptions of official Cheat Engine features.

## Current Status

| Item | Status |
|---|---|
| Current version | `v0.3.0 Preview` |
| Operating system | Windows 10 / Windows 11 |
| Architecture | x64 |
| UI language | Simplified Chinese / English (Chinese by default) |
| Main UI | Native WPF |
| DPI awareness | Per-Monitor V2 |
| Public release | `v0.2.0 Preview` published; `v0.3.0 Preview` package prepared |

## Features

- Lists applications with visible top-level windows and displays their icon, window title, process name, PID, and architecture.
- Attaches to and detaches from a selected target process.
- Provides common speed presets and custom multiplier input.
- Toggles acceleration through the main UI.
- Uses `LB + RB` as the default controller shortcut.
- Allows controller-button combinations to be changed from the settings window.
- Uses `Ctrl + F` as the default global keyboard shortcut without swallowing normal keyboard input.
- Allows the keyboard shortcut to be configured visually in Settings; the combination must include `Ctrl`, `Shift`, or `Alt`.
- Supports runtime Chinese/English switching from the language button at the top of the window.
- Supports an always-on-top main window and an optional compact status overlay.
- Synchronizes target audio with the selected multiplier by default, without a separate audio toggle.
- Re-renders cleanly when moved between displays with different DPI scaling.

## Screenshot

![FastMode main window](./assets/screenshot-main.png)

## Quick Start

1. Extract the complete package. Do not copy only `FastMode.exe`.
2. Run `FastMode.exe`.
3. Select the target application from the process list.
4. Click the attach button.
5. Select a preset multiplier, or enter a custom value and apply it.
6. Turn on acceleration from the UI, press the default `LB + RB` controller shortcut, or press the default `Ctrl + F` keyboard shortcut.
7. Disable acceleration or detach from the target before closing FastMode.

To use the English interface, click the language button immediately to the left of the topmost switch. FastMode starts in Simplified Chinese on every launch.

If the target application runs as administrator, FastMode will usually need to run with equal or higher privileges.

## Audio Synchronization

When process acceleration is enabled, audio follows the same multiplier automatically. There is no separate audio setting.

The native module currently covers or probes XAudio2, DirectSound, waveOut, WASAPI, and Windows Spatial Audio paths. The Spatial Audio implementation requests additional source samples through shadow buffers and uses SoundTouch time stretching to increase playback tempo while attempting to preserve the original pitch.

Applications use different audio engines, sample formats, and submission models. Audio compatibility therefore needs to be verified per target. Non-Spatial Audio paths may use different fallback behavior and are not guaranteed to provide the same pitch-preserving result.

## Compatibility and Limitations

- The current build supports x64 target processes only.
- Protected processes, anti-cheat environments, or applications that block remote-thread injection may not be attachable.
- Applications that do not use common Windows timing sources may not react correctly to the multiplier.
- Some applications finish audio initialization before FastMode is attached, so late-injection compatibility depends on the audio path.
- The current executable is not code-signed and Windows may display an unknown-publisher warning.
- Changing the runtime state of another process can cause compatibility issues. Save important work in the target application first.

## Version and Development Status

The current review build is `v0.3.0 Preview`, and its main features are still being verified against real target applications.

- The native WPF desktop application is the active development line.
- The Tauri/Web UI remains in the repository as an earlier implementation.
- x86 target-process support is outside the current release scope.
- The final public version number, branch policy, and compatibility list will be decided after review.

## Building from Source

### Desktop application

.NET 8 SDK is required:

```powershell
cd wpf\FastMode.Desktop
dotnet build -c Release
```

### Native Speedhack module

The current build script uses a MinGW-w64 x64 toolchain and statically links SoundTouch:

```powershell
powershell -ExecutionPolicy Bypass -File native\speedhack\build.ps1
```

The generated `speedhack_x64.dll` must be distributed alongside FastMode.

## Repository Layout

```text
fastmode/
├─ wpf/FastMode.Desktop/       Native WPF desktop application
├─ native/speedhack/           Native process and audio speed module
├─ src-tauri/resources/        Release location for the native DLL
└─ docs/                       Project documentation
```

The repository retains the earlier Tauri/Web UI implementation, but the native WPF application is the current daily-use and release target.

## Third-Party Projects and Credits

- [Cheat Engine](https://github.com/cheat-engine/cheat-engine): an important reference for the process Speedhack implementation.
- [Cheat Engine official website](https://cheatengine.org/): the official Cheat Engine website.
- [SoundTouch](https://www.surina.net/soundtouch/): tempo, pitch, and playback-rate processing.
- [MinHook](https://github.com/TsudaKageyu/minhook): a minimal Windows x86/x64 API hooking library.

Release packages should retain the third-party license texts in the `licenses` directory.

## Contributing

After the public repository is established, Issues can be used to report compatibility problems, target audio paths, controller models, and multi-display DPI results. Code contributions should include reproducible steps, the target architecture, and verification results whenever possible.

## License

The final open-source license for FastMode has not yet been selected and must be reviewed before public release.

Third-party components remain under their respective licenses. The current package includes the SoundTouch and MinHook license texts in its `licenses` directory. Because the native module references and uses the structure of Cheat Engine Speedhack, the applicable upstream licensing and redistribution requirements must also be reviewed and satisfied before source code or binaries are publicly released.

## Disclaimer

This project is intended for software compatibility research, personal testing, and runtime speed adjustment in controlled environments. Users are responsible for reviewing the target application's license terms, platform rules, and applicable laws, and assume all risks arising from use.
