# Runtime diagnosis 2026-07-30

## Connection refused root cause

1. Launched `src-tauri/target/debug/fastmode.exe` directly.
2. Debug Tauri binary embeds `devUrl=http://localhost:PORT` and loads UI from Vite dev server.
3. Strings in exe confirmed: contains `localhost:1420` (before fix).
4. `netstat` showed **nothing listening on 1420** — not primarily "port occupied", but **dev server never started**.
5. Result: WebView opens `localhost:PORT` → connection refused / cannot connect.

## Correct run modes

- Dev: `pnpm tauri dev` (starts Vite + Rust together)
- Standalone exe without Node: `pnpm tauri build` (release embeds `dist/`)

## Port change

- Old: 1420 / HMR 1421
- New: **38427** / HMR 38428 (rarer 5-digit, free on this machine at check time)
