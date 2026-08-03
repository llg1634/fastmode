//! Speedhack host: inject DLL + remote set speed.

use parking_lot::Mutex;
use serde::Serialize;
use std::path::{Path, PathBuf};
use std::sync::Arc;

#[derive(Debug, Clone, Serialize)]
pub struct AttachState {
    pub attached: bool,
    pub pid: Option<u32>,
    pub name: Option<String>,
    pub arch: Option<String>,
    pub enabled: bool,
    pub speed: f32,
    pub message: String,
}

impl Default for AttachState {
    fn default() -> Self {
        Self {
            attached: false,
            pid: None,
            name: None,
            arch: None,
            enabled: false,
            speed: 1.0,
            message: "未附加".into(),
        }
    }
}

#[allow(dead_code)]
struct Attached {
    pid: u32,
    name: String,
    arch: String,
    #[cfg(windows)]
    handle: isize,
    set_speed_addr: usize,
}

pub struct SpeedhackHost {
    inner: Mutex<Option<Attached>>,
    state: Mutex<AttachState>,
}

impl SpeedhackHost {
    pub fn new() -> Arc<Self> {
        Arc::new(Self {
            inner: Mutex::new(None),
            state: Mutex::new(AttachState::default()),
        })
    }

    pub fn state(&self) -> AttachState {
        self.state.lock().clone()
    }

    pub fn attach(
        &self,
        pid: u32,
        name: &str,
        arch: &str,
        resource_dir: &Path,
    ) -> Result<AttachState, String> {
        self.detach_silent();

        #[cfg(windows)]
        {
            let dll_name = if arch == "x86" {
                "speedhack_x86.dll"
            } else {
                "speedhack_x64.dll"
            };
            let dll_path = resolve_dll(resource_dir, dll_name)?;
            let handle = open_process(pid)?;
            inject_dll(handle, &dll_path)?;
            std::thread::sleep(std::time::Duration::from_millis(50));

            let (init_addr, set_speed_addr) =
                resolve_remote_exports(pid, &dll_path, dll_name)?;
            remote_call(handle, init_addr, 0)?;
            remote_call_f32(handle, set_speed_addr, 1.0)?;

            *self.inner.lock() = Some(Attached {
                pid,
                name: name.to_string(),
                arch: arch.to_string(),
                handle,
                set_speed_addr,
            });

            let st = AttachState {
                attached: true,
                pid: Some(pid),
                name: Some(name.to_string()),
                arch: Some(arch.to_string()),
                enabled: false,
                speed: 1.0,
                message: format!("已附加 {name} ({arch})"),
            };
            *self.state.lock() = st.clone();
            Ok(st)
        }

        #[cfg(not(windows))]
        {
            let _ = (pid, name, arch, resource_dir);
            Err("仅支持 Windows".into())
        }
    }

    pub fn detach(&self) -> AttachState {
        if let Some(att) = self.inner.lock().as_ref() {
            let _ = self.apply_speed_on(att, 1.0);
        }
        self.detach_silent();
        let st = AttachState {
            message: "已断开".into(),
            ..AttachState::default()
        };
        *self.state.lock() = st.clone();
        st
    }

    fn detach_silent(&self) {
        #[cfg(windows)]
        {
            if let Some(att) = self.inner.lock().take() {
                unsafe {
                    let _ = windows::Win32::Foundation::CloseHandle(
                        windows::Win32::Foundation::HANDLE(att.handle as *mut _),
                    );
                }
            }
        }
        #[cfg(not(windows))]
        {
            let _ = self.inner.lock().take();
        }
    }

    pub fn set_speed(&self, speed: f32, enabled: bool) -> Result<AttachState, String> {
        if !(speed.is_finite() && speed > 0.0) {
            return Err("倍速无效".into());
        }
        let effective = if enabled { speed } else { 1.0 };
        {
            let guard = self.inner.lock();
            let Some(att) = guard.as_ref() else {
                return Err("尚未附加进程".into());
            };
            self.apply_speed_on(att, effective)?;
        }
        let mut st = self.state.lock();
        st.speed = speed;
        st.enabled = enabled;
        st.message = if enabled {
            format!("加速已启用 · {speed:.3}x")
        } else {
            format!("加速已关闭 · 目标倍速 {speed:.3}x")
        };
        Ok(st.clone())
    }

    #[cfg(windows)]
    fn apply_speed_on(&self, att: &Attached, speed: f32) -> Result<(), String> {
        remote_call_f32(att.handle, att.set_speed_addr, speed)
    }

    #[cfg(not(windows))]
    fn apply_speed_on(&self, _att: &Attached, _speed: f32) -> Result<(), String> {
        Ok(())
    }
}

fn resolve_dll(resource_dir: &Path, name: &str) -> Result<PathBuf, String> {
    let mut candidates = vec![
        resource_dir.join(name),
        resource_dir.join("resources").join(name),
        PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .join("resources")
            .join(name),
    ];
    if let Ok(exe) = std::env::current_exe() {
        if let Some(dir) = exe.parent() {
            candidates.push(dir.join("resources").join(name));
            candidates.push(dir.join(name));
        }
    }
    for c in candidates {
        if c.exists() {
            return Ok(c.canonicalize().unwrap_or(c));
        }
    }
    Err(format!(
        "找不到 {name}。请先编译 native/speedhack 并复制到 src-tauri/resources/"
    ))
}

#[cfg(windows)]
fn open_process(pid: u32) -> Result<isize, String> {
    use windows::Win32::System::Threading::{
        OpenProcess, PROCESS_CREATE_THREAD, PROCESS_QUERY_INFORMATION, PROCESS_VM_OPERATION,
        PROCESS_VM_READ, PROCESS_VM_WRITE,
    };
    let access = PROCESS_CREATE_THREAD
        | PROCESS_QUERY_INFORMATION
        | PROCESS_VM_OPERATION
        | PROCESS_VM_WRITE
        | PROCESS_VM_READ;
    unsafe {
        let h = OpenProcess(access, false, pid).map_err(|e| {
            format!("打开进程失败（可能需要管理员权限或进程受保护）: {e}")
        })?;
        Ok(h.0 as isize)
    }
}

#[cfg(windows)]
fn inject_dll(process: isize, dll_path: &Path) -> Result<(), String> {
    use std::os::windows::ffi::OsStrExt;
    use windows::Win32::Foundation::{CloseHandle, HANDLE};
    use windows::Win32::System::Diagnostics::Debug::WriteProcessMemory;
    use windows::Win32::System::LibraryLoader::{GetModuleHandleW, GetProcAddress};
    use windows::Win32::System::Memory::{
        VirtualAllocEx, VirtualFreeEx, MEM_COMMIT, MEM_RELEASE, MEM_RESERVE, PAGE_READWRITE,
    };
    use windows::Win32::System::Threading::{
        CreateRemoteThread, GetExitCodeThread, WaitForSingleObject, INFINITE,
    };

    let wide: Vec<u16> = dll_path
        .as_os_str()
        .encode_wide()
        .chain(std::iter::once(0))
        .collect();
    let bytes = wide.len() * 2;
    let process = HANDLE(process as *mut _);

    unsafe {
        let remote = VirtualAllocEx(
            process,
            None,
            bytes,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_READWRITE,
        );
        if remote.is_null() {
            return Err("VirtualAllocEx 失败".into());
        }

        let mut written = 0usize;
        if WriteProcessMemory(
            process,
            remote,
            wide.as_ptr() as *const _,
            bytes,
            Some(&mut written),
        )
        .is_err()
            || written != bytes
        {
            let _ = VirtualFreeEx(process, remote, 0, MEM_RELEASE);
            return Err("WriteProcessMemory 失败".into());
        }

        let k32 = GetModuleHandleW(windows::core::w!("kernel32.dll"))
            .map_err(|e| format!("GetModuleHandle kernel32 失败: {e}"))?;
        let load_library = GetProcAddress(k32, windows::core::s!("LoadLibraryW"))
            .ok_or_else(|| "GetProcAddress LoadLibraryW 失败".to_string())?;

        let thread = match CreateRemoteThread(
            process,
            None,
            0,
            Some(std::mem::transmute(load_library)),
            Some(remote),
            0,
            None,
        ) {
            Ok(t) => t,
            Err(e) => {
                let _ = VirtualFreeEx(process, remote, 0, MEM_RELEASE);
                return Err(format!("CreateRemoteThread 失败: {e}"));
            }
        };

        let _ = WaitForSingleObject(thread, INFINITE);
        let mut code = 0u32;
        let _ = GetExitCodeThread(thread, &mut code);
        let _ = CloseHandle(thread);
        let _ = VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        if code == 0 {
            return Err("LoadLibraryW 返回空，DLL 注入失败（路径/架构/依赖问题）".into());
        }
        Ok(())
    }
}

#[cfg(windows)]
fn resolve_remote_exports(
    pid: u32,
    dll_path: &Path,
    dll_name: &str,
) -> Result<(usize, usize), String> {
    use std::os::windows::ffi::OsStrExt;
    use windows::Win32::System::LibraryLoader::{
        GetProcAddress, LoadLibraryW,
    };

    let wide: Vec<u16> = dll_path
        .as_os_str()
        .encode_wide()
        .chain(std::iter::once(0))
        .collect();

    unsafe {
        let local_mod = LoadLibraryW(windows::core::PCWSTR(wide.as_ptr()))
            .map_err(|e| format!("本地加载 DLL 失败: {e}"))?;
        let init_local = GetProcAddress(local_mod, windows::core::s!("FmInitThread"))
            .ok_or_else(|| "缺少导出 FmInitThread".to_string())? as usize;
        let set_local = GetProcAddress(local_mod, windows::core::s!("FmSetSpeedThread"))
            .ok_or_else(|| "缺少导出 FmSetSpeedThread".to_string())? as usize;
        let local_base = local_mod.0 as usize;
        let init_off = init_local.wrapping_sub(local_base);
        let set_off = set_local.wrapping_sub(local_base);
        let remote_base = find_remote_module_base(pid, dll_name)
            .ok_or_else(|| "注入后未找到远程模块".to_string())?;
        let _ = local_mod; // keep loaded for offset stability
        Ok((remote_base + init_off, remote_base + set_off))
    }
}

#[cfg(windows)]
fn find_remote_module_base(pid: u32, dll_name: &str) -> Option<usize> {
    use windows::Win32::Foundation::CloseHandle;
    use windows::Win32::System::Diagnostics::ToolHelp::{
        CreateToolhelp32Snapshot, Module32FirstW, Module32NextW, MODULEENTRY32W, TH32CS_SNAPMODULE,
        TH32CS_SNAPMODULE32,
    };

    unsafe {
        let snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid).ok()?;
        let mut me = MODULEENTRY32W {
            dwSize: std::mem::size_of::<MODULEENTRY32W>() as u32,
            ..Default::default()
        };
        let target = dll_name.to_lowercase();
        let mut ok = Module32FirstW(snap, &mut me).is_ok();
        while ok {
            let name = String::from_utf16_lossy(
                &me.szModule
                    .iter()
                    .take_while(|c| **c != 0)
                    .copied()
                    .collect::<Vec<_>>(),
            )
            .to_lowercase();
            if name == target {
                let base = me.modBaseAddr as usize;
                let _ = CloseHandle(snap);
                return Some(base);
            }
            ok = Module32NextW(snap, &mut me).is_ok();
        }
        let _ = CloseHandle(snap);
        None
    }
}

#[cfg(windows)]
fn remote_call(process: isize, addr: usize, param: usize) -> Result<(), String> {
    use windows::Win32::Foundation::{CloseHandle, HANDLE};
    use windows::Win32::System::Threading::{CreateRemoteThread, WaitForSingleObject, INFINITE};
    let process = HANDLE(process as *mut _);
    unsafe {
        let thread = CreateRemoteThread(
            process,
            None,
            0,
            Some(std::mem::transmute(addr)),
            if param == 0 {
                None
            } else {
                Some(param as *mut _)
            },
            0,
            None,
        )
        .map_err(|e| format!("远程调用失败: {e}"))?;
        let _ = WaitForSingleObject(thread, INFINITE);
        let _ = CloseHandle(thread);
        Ok(())
    }
}

#[cfg(windows)]
fn remote_call_f32(process: isize, addr: usize, value: f32) -> Result<(), String> {
    use windows::Win32::Foundation::{CloseHandle, HANDLE};
    use windows::Win32::System::Diagnostics::Debug::WriteProcessMemory;
    use windows::Win32::System::Memory::{
        VirtualAllocEx, VirtualFreeEx, MEM_COMMIT, MEM_RELEASE, MEM_RESERVE, PAGE_READWRITE,
    };
    use windows::Win32::System::Threading::{CreateRemoteThread, WaitForSingleObject, INFINITE};

    let process = HANDLE(process as *mut _);
    unsafe {
        let remote = VirtualAllocEx(process, None, 4, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if remote.is_null() {
            return Err("分配远程倍速参数失败".into());
        }
        let mut written = 0usize;
        let bytes = value.to_le_bytes();
        if WriteProcessMemory(
            process,
            remote,
            bytes.as_ptr() as *const _,
            4,
            Some(&mut written),
        )
        .is_err()
        {
            let _ = VirtualFreeEx(process, remote, 0, MEM_RELEASE);
            return Err("写入倍速参数失败".into());
        }
        let thread = match CreateRemoteThread(
            process,
            None,
            0,
            Some(std::mem::transmute(addr)),
            Some(remote),
            0,
            None,
        ) {
            Ok(t) => t,
            Err(e) => {
                let _ = VirtualFreeEx(process, remote, 0, MEM_RELEASE);
                return Err(format!("远程设速失败: {e}"));
            }
        };
        let _ = WaitForSingleObject(thread, INFINITE);
        let _ = CloseHandle(thread);
        let _ = VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        Ok(())
    }
}




