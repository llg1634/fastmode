# FastMode

Windows 加速小工具。当前主 UI：**WPF 原生（PCL 风格）**，不依赖本地 Web 端口。

## 推荐运行（原生）

```bash
cd wpf/FastMode.Desktop
dotnet run -c Release
```

或直接打开：

`wpf/FastMode.Desktop/bin/Release/net8.0-windows/FastMode.exe`

## 功能

- 选择并附加目标进程（当前内置 x64 DLL）
- 预设 / 自定义倍速
- 启用 / 关闭加速
- 窗口置顶
- 手柄组合键开关（默认 LB+RB，可录制；录制中右键恢复默认）
- 中文界面；关闭即退出
- **无 localhost / 拒绝连接问题**

## 加速 DLL

源码：`native/speedhack/`  
构建：`powershell -ExecutionPolicy Bypass -File native/speedhack/build.ps1`  
输出复制为 `wpf/FastMode.Desktop/speedhack_x64.dll`

## 旧 Tauri Web UI（保留，不推荐日常）

```bash
pnpm tauri dev   # 开发端口 38427
pnpm tauri build # 嵌入前端后可独立运行
```

Debug 直接双击 `target/debug/fastmode.exe` 会连 dev server，未启动时出现拒绝连接。

## 注意

- 反作弊/受保护进程可能无法注入
- 不使用标准时间 API 的游戏可能无效
- 可能需要管理员权限
