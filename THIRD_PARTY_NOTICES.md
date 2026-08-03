# 第三方声明（Third-Party Notices）

本软件（FastMode）以 [MIT License](LICENSE) 发布。以下第三方组件随本软件分发，各自遵循其原始许可证：

| 组件 | 许可证 | 源码与许可证文本位置 |
|---|---|---|
| MinHook（Tsuda Kageyu） | BSD 2-Clause | `native/speedhack/minhook-src/`（含 `LICENSE.txt`） |
| SoundTouch（Olli Parviainen） | GNU LGPL v2.1 | `native/speedhack/third_party/soundtouch/`（含 `COPYING.TXT`） |

## SoundTouch（LGPL-2.1）静态链接说明

本软件将 SoundTouch 以静态方式链接进 `speedhack_x64.dll`。根据 LGPL-2.1 第 6 节的要求：

- 随附 SoundTouch 完整源码（`native/speedhack/third_party/soundtouch/`）；
- 随附构建脚本（`native/speedhack/build.ps1`），允许使用修改后的 SoundTouch 重新构建并替换 `speedhack_x64.dll`。

## MinHook（BSD 2-Clause）

MinHook 源码包含 Hacker Disassembler Engine 32/64（Copyright (c) 2008-2009, Vyacheslav Patkov）的 BSD 声明，均保留在 `native/speedhack/minhook-src/LICENSE.txt` 中。

## 其他声明

本项目不包含也不分发 Cheat Engine 的任何代码或二进制文件。
