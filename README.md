# FastMode

Windows 加速小工具：从 Cheat Engine speedhack 思路独立实现时间 API 加速，提供清新简洁的桌面面板。

## 功能

- 选择并附加目标进程（64 位优先；32 位需自行编译 `speedhack_x86.dll`）
- 预设 / 自定义倍速
- 启用 / 关闭加速
- 窗口置顶
- 手柄组合键开关（默认 LB+RB，可录制修改）
- 中文界面；关闭窗口即退出

## 开发

### 依赖

- Node.js + pnpm
- Rust + MSVC 工具链（Tauri）
- 可选：MSYS2 mingw64 gcc（编译 speedhack DLL）

### 安装与运行

```bash
cd fastmode
pnpm install
# 编译 speedhack DLL（若 resources 中尚无）
powershell -ExecutionPolicy Bypass -File native/speedhack/build.ps1
pnpm tauri dev
```

### 构建

```bash
pnpm tauri build
```

## 结构

- `src/` React UI
- `src-tauri/` Rust 主机（进程、注入、XInput、置顶）
- `native/speedhack/` 注入用 DLL（MinHook + 时间缩放）
- `docs/UI_REQUIREMENTS.md` 冻结的 UI 需求

## 注意

- 部分受保护/反作弊进程无法注入
- 不使用标准时间 API 的游戏可能无效
- 可能需要管理员权限
- 请仅用于单机与合法测试场景
