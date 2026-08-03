use serde::Serialize;

#[derive(Debug, Clone, Serialize)]
pub struct ProcessInfo {
    pub pid: u32,
    pub name: String,
    pub arch: String,
}

#[cfg(windows)]
pub fn list_processes(query: &str) -> Result<Vec<ProcessInfo>, String> {
    use windows::Win32::Foundation::CloseHandle;
    use windows::Win32::System::Diagnostics::ToolHelp::{
        CreateToolhelp32Snapshot, Process32FirstW, Process32NextW, PROCESSENTRY32W,
        TH32CS_SNAPPROCESS,
    };

    unsafe {
        let snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
            .map_err(|e| format!("无法枚举进程: {e}"))?;
        let mut entry = PROCESSENTRY32W {
            dwSize: std::mem::size_of::<PROCESSENTRY32W>() as u32,
            ..Default::default()
        };
        let mut out = Vec::new();
        let q = query.trim().to_lowercase();

        let mut ok = Process32FirstW(snap, &mut entry).is_ok();
        while ok {
            let name = String::from_utf16_lossy(
                &entry
                    .szExeFile
                    .iter()
                    .take_while(|c| **c != 0)
                    .copied()
                    .collect::<Vec<_>>(),
            );
            if q.is_empty() || name.to_lowercase().contains(&q) {
                out.push(ProcessInfo {
                    pid: entry.th32ProcessID,
                    name,
                    arch: detect_arch(entry.th32ProcessID),
                });
            }
            ok = Process32NextW(snap, &mut entry).is_ok();
        }
        let _ = CloseHandle(snap);
        out.sort_by(|a, b| a.name.to_lowercase().cmp(&b.name.to_lowercase()));
        out.retain(|p| !p.name.is_empty());
        Ok(out)
    }
}

#[cfg(windows)]
fn detect_arch(pid: u32) -> String {
    use windows::Win32::Foundation::CloseHandle;
    use windows::Win32::System::Threading::{
        IsWow64Process, OpenProcess, PROCESS_QUERY_LIMITED_INFORMATION,
    };
    unsafe {
        let Ok(handle) = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, pid) else {
            return "unknown".into();
        };
        let mut wow = false.into();
        let arch = if IsWow64Process(handle, &mut wow).is_ok() {
            if wow.as_bool() { "x86".into() } else { "x64".into() }
        } else {
            "unknown".into()
        };
        let _ = CloseHandle(handle);
        arch
    }
}

#[cfg(not(windows))]
pub fn list_processes(_query: &str) -> Result<Vec<ProcessInfo>, String> {
    Err("仅支持 Windows".into())
}
