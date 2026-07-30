use parking_lot::Mutex;
use std::path::PathBuf;
use std::sync::Arc;
use tauri::{AppHandle, Emitter, Manager, State};

mod gamepad;
mod process;
mod settings;
mod speedhack_host;

use gamepad::{spawn_poll_loop, GamepadService, GamepadStatus};
use process::{list_processes as list_processes_impl, ProcessInfo};
use settings::{load_settings, persist_settings, AppSettings};
use speedhack_host::{AttachState, SpeedhackHost};

pub struct AppState {
    pub settings: Mutex<AppSettings>,
    pub host: Arc<SpeedhackHost>,
    pub gamepad: GamepadService,
    pub resource_dir: Mutex<PathBuf>,
}

fn emit_attach(app: &AppHandle, st: &AttachState) {
    let _ = app.emit("attach-state", st);
}

#[tauri::command]
fn list_processes(query: String) -> Result<Vec<ProcessInfo>, String> {
    list_processes_impl(&query)
}

#[tauri::command]
fn get_settings(state: State<AppState>) -> AppSettings {
    state.settings.lock().clone()
}

#[tauri::command]
fn save_settings(settings: AppSettings, state: State<AppState>) -> Result<AppSettings, String> {
    persist_settings(&settings)?;
    *state.settings.lock() = settings.clone();
    Ok(settings)
}

#[tauri::command]
fn set_always_on_top(enabled: bool, app: AppHandle, state: State<AppState>) -> Result<(), String> {
    if let Some(win) = app.get_webview_window("main") {
        win.set_always_on_top(enabled)
            .map_err(|e| format!("设置置顶失败: {e}"))?;
    }
    let mut s = state.settings.lock();
    s.always_on_top = enabled;
    persist_settings(&s)?;
    Ok(())
}

#[tauri::command]
fn get_gamepad_status(state: State<AppState>) -> GamepadStatus {
    state.gamepad.status()
}

#[tauri::command]
fn get_attach_state(state: State<AppState>) -> AttachState {
    let mut st = state.host.state();
    let s = state.settings.lock();
    if !st.attached {
        st.speed = s.current_speed;
        st.enabled = false;
    }
    st
}

#[tauri::command]
fn attach_process(pid: u32, app: AppHandle, state: State<AppState>) -> Result<AttachState, String> {
    let procs = list_processes_impl("")?;
    let proc = procs
        .into_iter()
        .find(|p| p.pid == pid)
        .ok_or_else(|| format!("找不到 PID {pid}"))?;
    let resource_dir = state.resource_dir.lock().clone();
    let st = state
        .host
        .attach(pid, &proc.name, &proc.arch, &resource_dir)?;
    {
        let mut s = state.settings.lock();
        s.last_process_name = Some(proc.name.clone());
        s.enabled = false;
        let _ = persist_settings(&s);
    }
    // apply stored speed only when enabled later
    emit_attach(&app, &st);
    Ok(st)
}

#[tauri::command]
fn detach_process(app: AppHandle, state: State<AppState>) -> Result<AttachState, String> {
    let st = state.host.detach();
    {
        let mut s = state.settings.lock();
        s.enabled = false;
        let _ = persist_settings(&s);
    }
    emit_attach(&app, &st);
    Ok(st)
}

#[tauri::command]
fn set_speed(speed: f32, app: AppHandle, state: State<AppState>) -> Result<AttachState, String> {
    if !(speed.is_finite() && speed > 0.0) {
        return Err("倍速必须是大于 0 的数字".into());
    }
    let enabled = {
        let mut s = state.settings.lock();
        s.current_speed = speed;
        let en = s.enabled;
        persist_settings(&s)?;
        en
    };
    let st = if state.host.state().attached {
        state.host.set_speed(speed, enabled)?
    } else {
        let mut st = state.host.state();
        st.speed = speed;
        st.enabled = false;
        st.message = format!("已记录倍速 {speed:.3}x（尚未附加）");
        st
    };
    emit_attach(&app, &st);
    Ok(st)
}

#[tauri::command]
fn set_enabled(enabled: bool, app: AppHandle, state: State<AppState>) -> Result<AttachState, String> {
    let speed = {
        let mut s = state.settings.lock();
        s.enabled = enabled;
        let sp = s.current_speed;
        persist_settings(&s)?;
        sp
    };
    if !state.host.state().attached {
        return Err("请先附加目标进程".into());
    }
    let st = state.host.set_speed(speed, enabled)?;
    emit_attach(&app, &st);
    Ok(st)
}

#[tauri::command]
fn set_hotkey_buttons(buttons: Vec<u32>, state: State<AppState>) -> Result<AppSettings, String> {
    let mut s = state.settings.lock();
    s.hotkey_buttons = if buttons.is_empty() { vec![4, 5] } else { buttons };
    persist_settings(&s)?;
    Ok(s.clone())
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let settings = load_settings();
    let host = SpeedhackHost::new();
    let gamepad = GamepadService::new();

    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .setup(move |app| {
            let resource_dir = app
                .path()
                .resource_dir()
                .unwrap_or_else(|_| PathBuf::from(env!("CARGO_MANIFEST_DIR")));

            // Apply always on top at start
            if let Some(win) = app.get_webview_window("main") {
                let _ = win.set_always_on_top(settings.always_on_top);
            }

            let state = AppState {
                settings: Mutex::new(settings),
                host: host.clone(),
                gamepad: gamepad.clone(),
                resource_dir: Mutex::new(resource_dir),
            };
            app.manage(state);

            // Gamepad poll loop
            let app_handle = app.handle().clone();
            let gp = gamepad.clone();
            let host2 = host.clone();
            spawn_poll_loop(
                gp,
                {
                    let app_handle = app_handle.clone();
                    move || {
                        app_handle
                            .try_state::<AppState>()
                            .map(|s| s.settings.lock().hotkey_buttons.clone())
                            .unwrap_or_else(|| vec![4, 5])
                    }
                },
                move |status, edge| {
                    let _ = app_handle.emit("gamepad-status", &status);
                    if edge {
                        if let Some(state) = app_handle.try_state::<AppState>() {
                            if !state.host.state().attached {
                                return;
                            }
                            let (speed, enabled_next) = {
                                let mut s = state.settings.lock();
                                let next = !s.enabled;
                                s.enabled = next;
                                let sp = s.current_speed;
                                let _ = persist_settings(&s);
                                (sp, next)
                            };
                            if let Ok(st) = host2.set_speed(speed, enabled_next) {
                                let _ = app_handle.emit("attach-state", &st);
                                let _ = app_handle.emit("hotkey-toggle", enabled_next);
                            }
                        }
                    }
                },
            );

            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            list_processes,
            get_settings,
            save_settings,
            set_always_on_top,
            get_gamepad_status,
            get_attach_state,
            attach_process,
            detach_process,
            set_speed,
            set_enabled,
            set_hotkey_buttons
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

