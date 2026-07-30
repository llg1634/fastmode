import { useEffect, useMemo, useState } from "react";
import clsx from "clsx";
import {
  attachProcess,
  detachProcess,
  formatHotkey,
  getAttachState,
  getGamepadStatus,
  getSettings,
  listProcesses,
  onAttachState,
  onGamepadStatus,
  saveSettings,
  setAlwaysOnTop,
  setEnabled,
  setHotkeyButtons,
  setSpeed,
  type AppSettings,
  type AttachState,
  type GamepadStatus,
  type ProcessInfo,
  BUTTON_NAMES,
} from "./lib/api";

type Page = "main" | "settings";

const DEFAULT_PRESETS = [0.5, 1, 2, 3, 5];

export default function App() {
  const [page, setPage] = useState<Page>("main");
  const [settings, setSettings] = useState<AppSettings | null>(null);
  const [attach, setAttach] = useState<AttachState | null>(null);
  const [gamepad, setGamepad] = useState<GamepadStatus>({ connected: false, name: null, buttons_pressed: [] });
  const [query, setQuery] = useState("");
  const [processes, setProcesses] = useState<ProcessInfo[]>([]);
  const [loadingList, setLoadingList] = useState(false);
  const [busy, setBusy] = useState(false);
  const [customSpeed, setCustomSpeed] = useState("2");
  const [error, setError] = useState<string | null>(null);
  const [recording, setRecording] = useState(false);
  const [recorded, setRecorded] = useState<number[]>([]);
  const [status, setStatus] = useState("就绪");

  async function refreshProcesses(q = query) {
    setLoadingList(true);
    try {
      const list = await listProcesses(q);
      setProcesses(list);
    } catch (e) {
      setError(String(e));
    } finally {
      setLoadingList(false);
    }
  }

  useEffect(() => {
    (async () => {
      try {
        const [s, a, g] = await Promise.all([getSettings(), getAttachState(), getGamepadStatus()]);
        setSettings(s);
        setAttach(a);
        setGamepad(g);
        setCustomSpeed(String(s.current_speed));
        await setAlwaysOnTop(s.always_on_top);
        await refreshProcesses("");
      } catch (e) {
        setError(String(e));
      }
    })();
    let u1: (() => void) | undefined;
    let u2: (() => void) | undefined;
    onAttachState((s) => {
      setAttach(s);
      setStatus(s.message || (s.attached ? "已附加" : "未附加"));
    }).then((u) => (u1 = u));
    onGamepadStatus((g) => {
      setGamepad(g);
      if (recording) {
        const pressed = g.buttons_pressed.slice().sort((a, b) => a - b);
        if (pressed.length) setRecorded(pressed);
      }
    }).then((u) => (u2 = u));
    return () => {
      u1?.();
      u2?.();
    };
  }, []);

  useEffect(() => {
    const t = setTimeout(() => refreshProcesses(query), 200);
    return () => clearTimeout(t);
  }, [query]);

  const presets = settings?.presets?.length ? settings.presets : DEFAULT_PRESETS;

  async function patchSettings( partial: Partial<AppSettings>) {
    if (!settings) return;
    const next = { ...settings, ...partial };
    const saved = await saveSettings(next);
    setSettings(saved);
    return saved;
  }

  async function onToggleTop(v: boolean) {
    await setAlwaysOnTop(v);
    await patchSettings({ always_on_top: v });
  }

  async function onAttach(pid: number) {
    setBusy(true);
    setError(null);
    try {
      const st = await attachProcess(pid);
      setAttach(st);
      setStatus(st.message);
      if (st.name) await patchSettings({ last_process_name: st.name });
    } catch (e) {
      setError(String(e));
    } finally {
      setBusy(false);
    }
  }

  async function onDetach() {
    setBusy(true);
    try {
      const st = await detachProcess();
      setAttach(st);
      setStatus(st.message);
    } catch (e) {
      setError(String(e));
    } finally {
      setBusy(false);
    }
  }

  async function onSetEnabled(v: boolean) {
    setBusy(true);
    setError(null);
    try {
      const st = await setEnabled(v);
      setAttach(st);
      await patchSettings({ enabled: v });
      setStatus(st.message);
    } catch (e) {
      setError(String(e));
    } finally {
      setBusy(false);
    }
  }

  async function onSetSpeed(speed: number) {
    if (!Number.isFinite(speed) || speed <= 0) {
      setError("倍速必须是大于 0 的数字");
      return;
    }
    setBusy(true);
    setError(null);
    try {
      const st = await setSpeed(speed);
      setAttach(st);
      setCustomSpeed(String(speed));
      await patchSettings({ current_speed: speed });
      setStatus(st.message);
    } catch (e) {
      setError(String(e));
    } finally {
      setBusy(false);
    }
  }

  async function saveHotkey() {
    const buttons = recorded.length ? recorded : [4, 5];
    const s = await setHotkeyButtons(buttons);
    setSettings(s);
    setRecording(false);
    setRecorded([]);
  }

  const selected = useMemo(() => {
    if (!attach?.pid) return null;
    return processes.find((p) => p.pid === attach.pid) || null;
  }, [attach, processes]);

  if (!settings || !attach) {
    return (
      <div className="app-shell">
        <div className="card">正在加载…</div>
      </div>
    );
  }

  if (page === "settings") {
    return (
      <div className="app-shell">
        <header className="topbar">
          <button className="link-btn" onClick={() => setPage("main")}>← 返回</button>
          <h1>设置</h1>
          <span />
        </header>

        <section className="card">
          <h2>倍速预设</h2>
          <p className="muted">主面板芯片使用这些数值（逗号分隔）。</p>
          <input
            className="input"
            defaultValue={presets.join(", ")}
            onBlur={async (e) => {
              const vals = e.target.value
                .split(/[,，\s]+/)
                .map((x) => parseFloat(x))
                .filter((n) => Number.isFinite(n) && n > 0)
                .slice(0, 8);
              if (vals.length) await patchSettings({ presets: vals });
            }}
          />
        </section>

        <section className="card">
          <h2>手柄快捷键</h2>
          <p className="muted">用于开关加速（不循环倍速）。默认 LB + RB。</p>
          <div className="row between">
            <div>
              <div className="label">当前组合</div>
              <div className="chips">
                {(recording ? recorded : settings.hotkey_buttons).map((b) => (
                  <span className="chip" key={b}>{BUTTON_NAMES[b] ?? b}</span>
                ))}
                {recording && !recorded.length && <span className="muted">请按下手柄键…</span>}
              </div>
            </div>
            <div className="row gap">
              {!recording ? (
                <button className="btn" onClick={() => { setRecording(true); setRecorded([]); }}>录制</button>
              ) : (
                <>
                  <button className="btn primary" onClick={saveHotkey}>保存</button>
                  <button className="btn" onClick={() => { setRecording(false); setRecorded([]); }}>取消</button>
                </>
              )}
              <button
                className="btn"
                onClick={async () => {
                  const s = await setHotkeyButtons([4, 5]);
                  setSettings(s);
                  setRecording(false);
                }}
              >恢复默认</button>
            </div>
          </div>
          {gamepad.connected ? (
            <p className="ok-text">已连接：{gamepad.name}</p>
          ) : (
            <p className="muted">未检测到手柄</p>
          )}
        </section>

        <section className="card">
          <h2>关于</h2>
          <p className="muted">FastMode 0.1.0 — Windows 加速小工具。时间 API 加速思路参考 Cheat Engine speedhack，独立实现。</p>
        </section>
      </div>
    );
  }

  return (
    <div className="app-shell">
      <header className="topbar">
        <div>
          <div className="brand">FastMode</div>
          <div className="muted tiny">清新加速控制面板</div>
        </div>
        <button className="icon-btn" title="设置" onClick={() => setPage("settings")}>⚙</button>
      </header>

      {error && <div className="banner error">{error}</div>}

      <section className="card">
        <div className="row between">
          <h2>目标进程</h2>
          <button className="btn" disabled={loadingList} onClick={() => refreshProcesses()}>
            {loadingList ? "刷新中…" : "刷新"}
          </button>
        </div>
        <input
          className="input"
          placeholder="搜索进程名…"
          value={query}
          onChange={(e) => setQuery(e.target.value)}
        />
        <div className="process-list">
          {processes.slice(0, 40).map((p) => (
            <button
              key={`${p.pid}-${p.name}`}
              className={clsx("process-item", attach.pid === p.pid && "active")}
              onClick={() => onAttach(p.pid)}
              disabled={busy}
            >
              <span className="name">{p.name}</span>
              <span className="meta">PID {p.pid} · {p.arch}</span>
            </button>
          ))}
          {!processes.length && <div className="muted pad">没有匹配进程</div>}
        </div>
        <div className="row between mt">
          <div className="muted tiny">
            {attach.attached
              ? `已附加：${attach.name ?? ""} (PID ${attach.pid}) · ${attach.arch}`
              : selected
                ? `选中：${selected.name}`
                : "未附加"}
          </div>
          {attach.attached && (
            <button className="btn" disabled={busy} onClick={onDetach}>断开</button>
          )}
        </div>
      </section>

      <section className="card">
        <div className="row between">
          <div>
            <h2>加速</h2>
            <p className="muted">启用后按当前倍速生效；关闭回到 1.0x</p>
          </div>
          <label className={clsx("switch", attach.enabled && "on")}> 
            <input
              type="checkbox"
              checked={attach.enabled}
              disabled={busy || !attach.attached}
              onChange={(e) => onSetEnabled(e.target.checked)}
            />
            <span />
          </label>
        </div>

        <div className="label mt">倍速</div>
        <div className="chips wrap">
          {presets.map((s) => (
            <button
              key={s}
              className={clsx("chip btn-chip", Math.abs((attach.speed || settings.current_speed) - s) < 1e-6 && "selected")}
              disabled={busy}
              onClick={() => onSetSpeed(s)}
            >
              {s}x
            </button>
          ))}
        </div>
        <div className="row gap mt">
          <input
            className="input grow"
            value={customSpeed}
            onChange={(e) => setCustomSpeed(e.target.value)}
            onKeyDown={(e) => {
              if (e.key === "Enter") onSetSpeed(parseFloat(customSpeed));
            }}
            placeholder="自定义倍速"
          />
          <button className="btn primary" disabled={busy} onClick={() => onSetSpeed(parseFloat(customSpeed))}>
            应用
          </button>
        </div>
        <div className="muted tiny mt">当前：{(attach.speed || settings.current_speed).toFixed(3)}x · {attach.enabled ? "已启用" : "未启用"}</div>
      </section>

      <section className="card">
        <div className="row between">
          <div>
            <h2>窗口</h2>
            <p className="muted">始终置顶，方便边玩边看</p>
          </div>
          <label className={clsx("switch", settings.always_on_top && "on")}> 
            <input
              type="checkbox"
              checked={settings.always_on_top}
              onChange={(e) => onToggleTop(e.target.checked)}
            />
            <span />
          </label>
        </div>
      </section>

      <section className="card">
        <div className="row between">
          <div>
            <h2>手柄</h2>
            <p className="muted">
              {gamepad.connected ? `已连接：${gamepad.name}` : "未连接"}
            </p>
            <p className="tiny">切换键：{formatHotkey(settings.hotkey_buttons)}</p>
          </div>
          <button className="btn" onClick={() => setPage("settings")}>修改</button>
        </div>
      </section>

      <footer className="statusbar">{status}</footer>

      <style>{`
        .app-shell { max-width: 480px; margin: 0 auto; padding: 14px 14px 18px; display:flex; flex-direction:column; gap:12px; }
        .topbar { display:flex; align-items:center; justify-content:space-between; gap:12px; }
        .topbar h1 { font-size: 16px; margin:0; font-weight:650; }
        .brand { font-size: 18px; font-weight: 700; letter-spacing: 0.2px; }
        .card { background: var(--bg-card); border: 1px solid var(--border); border-radius: 16px; padding: 14px; box-shadow: var(--shadow); }
        .card h2 { margin: 0; font-size: 14px; font-weight: 650; }
        .muted { color: var(--text-secondary); font-size: 12px; margin: 4px 0 0; }
        .tiny { font-size: 12px; color: var(--text-tertiary); }
        .ok-text { color: var(--ok); font-size: 12px; margin-top: 8px; }
        .row { display:flex; align-items:center; }
        .between { justify-content: space-between; gap: 10px; }
        .gap { gap: 8px; }
        .mt { margin-top: 10px; }
        .grow { flex: 1; }
        .input { width: 100%; margin-top: 8px; border: 1px solid var(--border); background: var(--bg-muted); border-radius: 12px; padding: 10px 12px; outline: none; }
        .input:focus { border-color: #9fd5b5; background: #fff; box-shadow: 0 0 0 3px var(--accent-soft); }
        .btn, .icon-btn, .link-btn { border: 1px solid var(--border); background: #fff; border-radius: 12px; padding: 8px 12px; cursor: pointer; color: var(--text); }
        .btn:hover, .icon-btn:hover { background: var(--bg-muted); }
        .btn:disabled { opacity: 0.55; cursor: not-allowed; }
        .btn.primary { background: var(--accent); border-color: var(--accent); color: #fff; }
        .btn.primary:hover { filter: brightness(0.96); }
        .icon-btn { width: 36px; height: 36px; display:grid; place-items:center; border-radius: 12px; }
        .link-btn { background: transparent; border: none; color: var(--text-secondary); padding: 4px 0; }
        .process-list { margin-top: 8px; max-height: 160px; overflow: auto; border: 1px solid var(--border); border-radius: 12px; background: #fbfcfb; }
        .process-item { width: 100%; text-align: left; border: 0; border-bottom: 1px solid var(--border); background: transparent; padding: 8px 10px; display:flex; justify-content:space-between; gap:8px; cursor:pointer; }
        .process-item:last-child { border-bottom: 0; }
        .process-item:hover { background: var(--accent-soft); }
        .process-item.active { background: var(--accent-soft); }
        .process-item .name { font-size: 13px; font-weight: 600; }
        .process-item .meta { font-size: 11px; color: var(--text-tertiary); }
        .pad { padding: 12px; }
        .chips { display:flex; gap: 6px; flex-wrap: wrap; align-items:center; }
        .chip { display:inline-flex; align-items:center; border-radius: 999px; padding: 4px 10px; background: var(--bg-muted); border: 1px solid var(--border); font-size: 12px; }
        .btn-chip { cursor: pointer; }
        .btn-chip.selected { background: var(--accent-soft); border-color: #8fceb0; color: #1f6b45; font-weight: 650; }
        .label { font-size: 12px; color: var(--text-secondary); }
        .switch { position: relative; width: 46px; height: 28px; display:inline-flex; }
        .switch input { opacity: 0; width: 0; height: 0; }
        .switch span { position:absolute; inset:0; background: #d7ddd9; border-radius: 999px; transition: .15s ease; }
        .switch span:before { content:""; position:absolute; width:22px; height:22px; left:3px; top:3px; background:#fff; border-radius:50%; transition:.15s ease; box-shadow: 0 1px 3px rgba(0,0,0,.15); }
        .switch.on span { background: var(--accent); }
        .switch.on span:before { transform: translateX(18px); }
        .banner.error { background: #fff1f0; color: var(--danger); border: 1px solid #f3c1c1; border-radius: 12px; padding: 10px 12px; font-size: 12px; }
        .statusbar { font-size: 12px; color: var(--text-secondary); padding: 2px 4px 8px; }
      `}</style>
    </div>
  );
}
