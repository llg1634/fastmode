# FastMode UI Requirements (Frozen)

> Status: **Frozen** (2026-07-30)  
> Visual refs: grok-app + fluxdo  
> Stack: Tauri 2 + React + Rust + native speedhack DLL

## Frozen decisions

| Topic | Decision |
|-------|----------|
| Accent | Light green, used sparingly |
| Window frame | System title bar |
| Process picker | Same-page search list; correctness first |
| Gamepad | Toggle enable/disable only |
| Mini bar | No |
| Language | Chinese first |
| Close | Quit app |

## Main panel

1. Header + settings
2. Process search / attach
3. Speedhack enable switch
4. Speed presets + custom
5. Always on top
6. Gamepad status + hotkey summary
7. Status bar

## Settings

- Speed presets
- Gamepad hotkey rebind
- About

## Defaults

- Window ~420x640
- Presets: 0.5 / 1 / 2 / 3 / 5
- Hotkey: LB + RB
- Close quits; no tray
