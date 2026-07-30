# FastMode UI Requirements (Draft for Alignment)

> Status: draft — waiting for user confirmation  
> Visual refs: grok-app (desktop chrome / density / settings rows), fluxdo (settings groups / shortcut rebind flow)  
> Product: Windows-only speed control utility (Cheat Engine speedhack capability, slim app)

## 1. Product UI Goal

清新、简洁、克制的桌面小工具，不是大工作台，也不是手机论坛客户端。

- 一眼能完成：选进程 → 开加速 → 调倍速
- 次级：置顶、手柄快捷键、预设
- 视觉：浅色为主可切深色；大圆角；少装饰；高可读
- 体量：紧凑控制面板，不是 1200px IDE

## 2. Window Chrome

| Item | Proposal | Need confirm? |
|------|----------|---------------|
| Default size | ~420 × 640 | yes |
| Min size | ~360 × 520 | soft |
| Resizable | yes, limited | soft |
| Start position | center monito / last position | soft |
| Title | FastMode | soft |
| Frame | native decorations first (simpler); optional frameless later like grok | **confirm** |
| Always on top | first-class toggle on main page | fixed from earlier req |
| Close behavior | quit app (v1); tray later optional | **confirm** |
| Single instance | yes | soft |

## 3. Information Architecture

Two levels only:

1. **Main panel** — daily use
2. **Settings** — hotkeys / advanced / about

No sidebar navigation like grok workbench. No bottom tab bar like fluxdo mobile.

Optional later: compact “mini bar” mode (speed only + status).

## 4. Main Panel Layout (top → bottom)

```
┌─────────────────────────────────────┐
│ FastMode                    [⚙]    │  header
├─────────────────────────────────────┤
│ 目标进程                            │
│ [搜索进程..................] [刷新] │
│ ┌ 选中: game.exe  PID 1234       ┐ │
│ │ 状态: 未附加 / 已附加 / 失败    │ │
│ └ [附加] / [断开]                ┘ │
├─────────────────────────────────────┤
│ 加速                                │
│  ( 启用 Speedhack )     [开关]     │
│                                     │
│ 倍速                                │
│  [0.5] [1] [2] [3] [5] [自定义]    │
│  自定义: [ 2.50 ] 倍   [应用]      │
│  当前: 2.00x                        │
├─────────────────────────────────────┤
│ 窗口                                │
│  ( 始终置顶 )           [开关]     │
├─────────────────────────────────────┤
│ 手柄                                │
│  已连接: Xbox ...                   │
│  切换键: LB + RB          [修改]   │
│  最近触发: —                        │
├─────────────────────────────────────┤
│ 状态栏: 就绪 / 注入中 / 错误信息    │
└─────────────────────────────────────┘
```

### Main controls detail

| Block | UI | Behavior |
|-------|----|----------|
| Process | search field + list/dropdown + refresh | filter by name; show pid, arch (32/64) |
| Attach | primary button | attach/inject; show success/fail |
| Enable | switch | enable/disable speedhack (disable = speed 1.0, keep hooks or teardown policy later) |
| Presets | chip/segment buttons | one tap set speed; selected chip highlighted |
| Custom speed | number input + apply | allow decimals; validate >0; reject NaN |
| Always on top | switch | immediate window effect + persist |
| Gamepad status | readonly summary | device name / disconnected |
| Hotkey summary | text + modify | opens rebind flow |
| Settings gear | icon button | opens settings page/sheet |

## 5. Settings UI

Style: fluxdo-like grouped cards + grok-like row (title / description / control).

### Groups

**A. Speed presets**
- edit 5 preset values (default 0.5 / 1 / 2 / 3 / 5)
- optional: cycle order used by gamepad toggle

**B. Gamepad hotkeys**
- Toggle / cycle speed combo (default LB+RB)
- optional later: Speed+ , Speed- , preset1..n
- each row: action name, current combo badges, [录制] [清空]

**C. Gamepad options**
- deadzone (if sticks ever used)
- require chord (all keys down) vs sequence
- trigger edge only (press moment, not hold spam)

**D. Window**
- always on top (duplicate of main, synced)
- start with last process optional

**E. About**
- version, open source notes

## 6. Hotkey Rebind Flow (critical UX)

Inspired by fluxdo shortcut settings, adapted for **gamepad buttons**:

1. User clicks **修改** on a hotkey row
2. Overlay/dialog: “请按下手柄组合键…”
3. Capture currently pressed gamepad buttons (standard map names: LB/RB/A/B/…)
4. Live preview chips: `LB` `RB`
5. Confirm on release stability or explicit **保存**
6. Conflict check if same combo used elsewhere
7. **恢复默认** available

Rules:
- Support multi-button chord (e.g. LB+RB, RB+RT)
- Show friendly names, store button indices internally
- Keyboard hotkeys: **out of scope for v1** unless requested
- Works even when main window focused; later: global listen while game focused (needed for real use)

## 7. Visual Design Tokens (from refs, simplified)

| Token | Proposal |
|-------|----------|
| Theme | light default + dark toggle |
| Radius | 12–16px cards, 999px chips/switches |
| Density | comfortable, not tiny CE classic |
| Color | neutral gray surface; one accent (blue/teal, not loud purple) |
| Type | clean sans; clear hierarchy title / body / meta |
| Elevation | flat + light border; minimal shadow |
| Icons | simple line icons (settings, refresh, gamepad) |
| Motion | short 150–200ms fades; no heavy parallax |

Do **not** copy:
- grok full chat workbench / big empty hero
- fluxdo mobile bottom tabs / social content chrome

## 8. States & Feedback

| State | UI |
|-------|----|
| No process | attach disabled; hint text |
| Attaching | button loading |
| Attached idle | green/ok status |
| Speed active | show current multiplier prominently |
| Gamepad missing | hotkey still configurable; warn on use |
| Inject fail | error banner + short reason |
| Unsupported arch | explicit message |

## 9. Persistence

Save locally:
- window always on top
- last speed / presets
- hotkey combos
- last process name (optional reattach)
- theme

## 10. Non-goals (v1 UI)

- Full CE feature surface
- Trainer builder
- Fancy controller SVG playground (optional later)
- Multi-language full i18n (Chinese first ok)
- Mobile layout

## 11. Open Questions for User

1. 主色偏 **浅蓝** 还是 **青绿/中性黑白**？
2. 窗口要 **系统标题栏** 还是 **无边框自定义标题栏**（更像 grok）？
3. 进程选择用 **下拉+搜索** 还是 **独立列表页/弹层**？
4. 手柄切换是 **在多档倍速间循环**，还是 **只开关加速**，还是 **两者都要**？
5. v1 要不要 **迷你悬浮条**（只显示倍速+开关）？
6. 文案语言： **仅中文** 还是中英可切？
7. 关闭窗口： **直接退出** 还是 **最小化到托盘**？
