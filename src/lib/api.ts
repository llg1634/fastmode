import { invoke } from "@tauri-apps/api/core";
import { listen, type UnlistenFn } from "@tauri-apps/api/event";

export type ProcessInfo = {
  pid: number;
  name: string;
  arch: "x86" | "x64" | "unknown";
};

export type AppSettings = {
  always_on_top: boolean;
  presets: number[];
  current_speed: number;
  enabled: boolean;
  hotkey_buttons: number[];
  last_process_name: string | null;
  theme: "light";
};

export type GamepadStatus = {
  connected: boolean;
  name: string | null;
  buttons_pressed: number[];
};

export type AttachState = {
  attached: boolean;
  pid: number | null;
  name: string | null;
  arch: string | null;
  enabled: boolean;
  speed: number;
  message: string;
};

export const BUTTON_NAMES: Record<number, string> = {
  0: "A",
  1: "B",
  2: "X",
  3: "Y",
  4: "LB",
  5: "RB",
  6: "LT",
  7: "RT",
  8: "Back",
  9: "Start",
  10: "L3",
  11: "R3",
  12: "上",
  13: "下",
  14: "左",
  15: "右",
  16: "Guide",
};

export function formatHotkey(buttons: number[]): string {
  if (!buttons.length) return "未设置";
  return buttons.map((b) => BUTTON_NAMES[b] ?? `#${b}`).join(" + ");
}

export async function listProcesses(query = ""): Promise<ProcessInfo[]> {
  return invoke("list_processes", { query });
}

export async function getSettings(): Promise<AppSettings> {
  return invoke("get_settings");
}

export async function saveSettings(settings: AppSettings): Promise<AppSettings> {
  return invoke("save_settings", { settings });
}

export async function setAlwaysOnTop(enabled: boolean): Promise<void> {
  return invoke("set_always_on_top", { enabled });
}

export async function getGamepadStatus(): Promise<GamepadStatus> {
  return invoke("get_gamepad_status");
}

export async function getAttachState(): Promise<AttachState> {
  return invoke("get_attach_state");
}

export async function attachProcess(pid: number): Promise<AttachState> {
  return invoke("attach_process", { pid });
}

export async function detachProcess(): Promise<AttachState> {
  return invoke("detach_process");
}

export async function setSpeed(speed: number): Promise<AttachState> {
  return invoke("set_speed", { speed });
}

export async function setEnabled(enabled: boolean): Promise<AttachState> {
  return invoke("set_enabled", { enabled });
}

export async function setHotkeyButtons(buttons: number[]): Promise<AppSettings> {
  return invoke("set_hotkey_buttons", { buttons });
}

export function onAttachState(cb: (s: AttachState) => void): Promise<UnlistenFn> {
  return listen<AttachState>("attach-state", (e) => cb(e.payload));
}

export function onGamepadStatus(cb: (s: GamepadStatus) => void): Promise<UnlistenFn> {
  return listen<GamepadStatus>("gamepad-status", (e) => cb(e.payload));
}

export function onHotkeyToggle(cb: (enabled: boolean) => void): Promise<UnlistenFn> {
  return listen<boolean>("hotkey-toggle", (e) => cb(e.payload));
}
