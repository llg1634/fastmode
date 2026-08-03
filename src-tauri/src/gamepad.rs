use parking_lot::Mutex;
use serde::Serialize;
use std::sync::Arc;
use std::time::Duration;

#[derive(Debug, Clone, Serialize, Default)]
pub struct GamepadStatus {
    pub connected: bool,
    pub name: Option<String>,
    pub buttons_pressed: Vec<u32>,
}

#[derive(Default)]
struct GamepadInner {
    status: GamepadStatus,
    prev_chord: bool,
}

#[derive(Clone, Default)]
pub struct GamepadService {
    inner: Arc<Mutex<GamepadInner>>,
}

impl GamepadService {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn status(&self) -> GamepadStatus {
        self.inner.lock().status.clone()
    }

    pub fn poll_once(&self, hotkey: &[u32]) -> (GamepadStatus, bool) {
        let (status, chord) = read_gamepad();
        let mut g = self.inner.lock();
        let edge = chord && !g.prev_chord;
        g.prev_chord = chord;
        g.status = status.clone();
        // hotkey unused in read - chord computed with hotkey
        let _ = hotkey;
        (status, edge)
    }

    pub fn poll_with_hotkey(&self, hotkey: &[u32]) -> (GamepadStatus, bool) {
        let (mut status, raw_buttons) = read_gamepad_raw();
        let chord = !hotkey.is_empty() && hotkey.iter().all(|b| raw_buttons.contains(b));
        status.buttons_pressed = raw_buttons;
        let mut g = self.inner.lock();
        let edge = chord && !g.prev_chord;
        g.prev_chord = chord;
        g.status = status.clone();
        (status, edge)
    }
}

#[cfg(windows)]
fn read_gamepad() -> (GamepadStatus, bool) {
    let (s, _) = read_gamepad_raw();
    (s, false)
}

#[cfg(windows)]
fn read_gamepad_raw() -> (GamepadStatus, Vec<u32>) {
    use windows::Win32::UI::Input::XboxController::{XInputGetState, XINPUT_STATE};

    // Try first 4 slots
    for i in 0..4u32 {
        let mut state = XINPUT_STATE::default();
        let r = unsafe { XInputGetState(i, &mut state) };
        if r == 0 {
            let buttons = xinput_to_standard(state.Gamepad.wButtons.0, state.Gamepad.bLeftTrigger, state.Gamepad.bRightTrigger);
            return (
                GamepadStatus {
                    connected: true,
                    name: Some(format!("XInput Controller #{i}")),
                    buttons_pressed: buttons.clone(),
                },
                buttons,
            );
        }
    }
    (
        GamepadStatus {
            connected: false,
            name: None,
            buttons_pressed: vec![],
        },
        vec![],
    )
}

#[cfg(windows)]
fn xinput_to_standard(wbuttons: u16, lt: u8, rt: u8) -> Vec<u32> {
    // Map XInput wButtons to standard gamepad indices (W3C)
    // A=0 B=1 X=2 Y=3 LB=4 RB=5 LT=6 RT=7 Back=8 Start=9 L3=10 R3=11
    // DUp=12 DDown=13 DLeft=14 DRight=15
    const DPAD_UP: u16 = 0x0001;
    const DPAD_DOWN: u16 = 0x0002;
    const DPAD_LEFT: u16 = 0x0004;
    const DPAD_RIGHT: u16 = 0x0008;
    const START: u16 = 0x0010;
    const BACK: u16 = 0x0020;
    const LEFT_THUMB: u16 = 0x0040;
    const RIGHT_THUMB: u16 = 0x0080;
    const LB: u16 = 0x0100;
    const RB: u16 = 0x0200;
    const A: u16 = 0x1000;
    const B: u16 = 0x2000;
    const X: u16 = 0x4000;
    const Y: u16 = 0x8000;

    let mut v = Vec::new();
    let map = [
        (A, 0u32),
        (B, 1),
        (X, 2),
        (Y, 3),
        (LB, 4),
        (RB, 5),
        (BACK, 8),
        (START, 9),
        (LEFT_THUMB, 10),
        (RIGHT_THUMB, 11),
        (DPAD_UP, 12),
        (DPAD_DOWN, 13),
        (DPAD_LEFT, 14),
        (DPAD_RIGHT, 15),
    ];
    for (mask, idx) in map {
        if wbuttons & mask != 0 {
            v.push(idx);
        }
    }
    if lt > 30 {
        v.push(6);
    }
    if rt > 30 {
        v.push(7);
    }
    v
}

#[cfg(not(windows))]
fn read_gamepad_raw() -> (GamepadStatus, Vec<u32>) {
    (GamepadStatus::default(), vec![])
}

pub fn spawn_poll_loop<F>(svc: GamepadService, get_hotkey: F, on_update: impl Fn(GamepadStatus, bool) + Send + 'static)
where
    F: Fn() -> Vec<u32> + Send + 'static,
{
    std::thread::spawn(move || loop {
        let hotkey = get_hotkey();
        let (status, edge) = svc.poll_with_hotkey(&hotkey);
        on_update(status, edge);
        std::thread::sleep(Duration::from_millis(33));
    });
}

