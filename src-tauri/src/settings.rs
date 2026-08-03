use serde::{Deserialize, Serialize};
use std::fs;
use std::path::PathBuf;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AppSettings {
    pub always_on_top: bool,
    pub presets: Vec<f32>,
    pub current_speed: f32,
    pub enabled: bool,
    pub hotkey_buttons: Vec<u32>,
    pub last_process_name: Option<String>,
    pub theme: String,
}

impl Default for AppSettings {
    fn default() -> Self {
        Self {
            always_on_top: false,
            presets: vec![0.5, 1.0, 2.0, 3.0, 5.0],
            current_speed: 2.0,
            enabled: false,
            hotkey_buttons: vec![4, 5], // LB + RB
            last_process_name: None,
            theme: "light".into(),
        }
    }
}

fn settings_path() -> PathBuf {
    let base = directories::ProjectDirs::from("com", "fastmode", "FastMode")
        .map(|p| p.config_dir().to_path_buf())
        .unwrap_or_else(|| PathBuf::from("."));
    let _ = fs::create_dir_all(&base);
    base.join("settings.json")
}

pub fn load_settings() -> AppSettings {
    let path = settings_path();
    match fs::read_to_string(&path) {
        Ok(s) => serde_json::from_str(&s).unwrap_or_default(),
        Err(_) => AppSettings::default(),
    }
}

pub fn persist_settings(settings: &AppSettings) -> Result<(), String> {
    let path = settings_path();
    let text = serde_json::to_string_pretty(settings).map_err(|e| e.to_string())?;
    fs::write(path, text).map_err(|e| e.to_string())
}

