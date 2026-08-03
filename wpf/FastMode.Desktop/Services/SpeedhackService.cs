using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using FastMode.Models;

namespace FastMode.Services;

public sealed class SpeedhackService : IDisposable
{
    private IntPtr _process = IntPtr.Zero;
    private IntPtr _setSpeedAddr = IntPtr.Zero;
    private bool _attached;

    public AttachInfo State { get; private set; } = new();

    public AttachInfo Attach(ProcessItem proc)
    {
        Detach(silent: true);

        if (proc.Arch == "x86")
            throw new InvalidOperationException(LocalizationService.Get("L10n.ErrorX86Unsupported"));

        var dllPath = ResolveDll("speedhack_x64.dll");
        var handle = OpenProcess(
            PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
            false, proc.Pid);
        if (handle == IntPtr.Zero)
            throw new InvalidOperationException(LocalizationService.Format(
                "L10n.ErrorOpenProcessTemplate", Marshal.GetLastWin32Error()));

        try
        {
            InjectDll(handle, dllPath);
            Thread.Sleep(50);
            var (initAddr, setAddr) = ResolveExports(handle, proc.Pid, dllPath, "speedhack_x64.dll");
            RemoteCall(handle, initAddr, IntPtr.Zero);
            RemoteCallFloat(handle, setAddr, 1f);

            _process = handle;
            _setSpeedAddr = setAddr;
            _attached = true;
            State = new AttachInfo
            {
                Attached = true,
                Pid = proc.Pid,
                Name = proc.Name,
                Arch = proc.Arch,
                Enabled = false,
                Speed = 1f,
                Message = LocalizationService.Format("L10n.AttachedTemplate", proc.Name, proc.Arch)
            };
            return State;
        }
        catch
        {
            CloseHandle(handle);
            throw;
        }
    }

    public AttachInfo Detach(bool silent = false)
    {
        if (_attached && _process != IntPtr.Zero && _setSpeedAddr != IntPtr.Zero)
        {
            try { RemoteCallFloat(_process, _setSpeedAddr, 1f); } catch { /* ignore */ }
        }
        if (_process != IntPtr.Zero)
        {
            CloseHandle(_process);
            _process = IntPtr.Zero;
        }
        _setSpeedAddr = IntPtr.Zero;
        _attached = false;
        State = new AttachInfo
        {
            Message = LocalizationService.Get(silent ? "L10n.NotAttached" : "L10n.Detached")
        };
        return State;
    }

    public AttachInfo SetSpeed(float speed, bool enabled)
    {
        if (!(speed > 0) || float.IsNaN(speed) || float.IsInfinity(speed))
            throw new ArgumentException(LocalizationService.Get("L10n.ErrorPositiveSpeed"));
        if (!_attached || _process == IntPtr.Zero || _setSpeedAddr == IntPtr.Zero)
            throw new InvalidOperationException(LocalizationService.Get("L10n.ErrorNoAttachedProcess"));

        var effective = enabled ? speed : 1f;
        RemoteCallFloat(_process, _setSpeedAddr, effective);
        State.Speed = speed;
        State.Enabled = enabled;
        State.Message = enabled
            ? LocalizationService.Format("L10n.SpeedEnabledTemplate", speed)
            : LocalizationService.Format("L10n.SpeedDisabledTemplate", speed);
        return State;
    }

    private static string ResolveDll(string name)
    {
        var candidates = new[]
        {
            Path.Combine(AppContext.BaseDirectory, name),
            Path.Combine(AppContext.BaseDirectory, "resources", name),
            Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, @"..\..\..\..\src-tauri\resources", name)),
            Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, @"..\..\..\..\..\src-tauri\resources", name)),
        };
        foreach (var c in candidates)
        {
            if (File.Exists(c)) return Path.GetFullPath(c);
        }
        throw new FileNotFoundException(LocalizationService.Format("L10n.ErrorDllMissingTemplate", name));
    }

    private static void InjectDll(IntPtr process, string dllPath)
    {
        var bytes = Encoding.Unicode.GetBytes(dllPath + "\0");
        var remote = VirtualAllocEx(process, IntPtr.Zero, (nuint)bytes.Length, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (remote == IntPtr.Zero) throw new InvalidOperationException(LocalizationService.Get("L10n.ErrorVirtualAlloc"));

        try
        {
            if (!WriteProcessMemory(process, remote, bytes, bytes.Length, out var written) || written != bytes.Length)
                throw new InvalidOperationException(LocalizationService.Get("L10n.ErrorWriteProcess"));

            var k32 = GetModuleHandle("kernel32.dll");
            var loadLibrary = GetProcAddress(k32, "LoadLibraryW");
            if (loadLibrary == IntPtr.Zero) throw new InvalidOperationException(LocalizationService.Get("L10n.ErrorLoadLibraryAddress"));

            var thread = CreateRemoteThread(process, IntPtr.Zero, 0, loadLibrary, remote, 0, out _);
            if (thread == IntPtr.Zero) throw new InvalidOperationException(LocalizationService.Format(
                "L10n.ErrorCreateRemoteThreadTemplate", Marshal.GetLastWin32Error()));
            WaitForSingleObject(thread, 0xFFFFFFFF);
            GetExitCodeThread(thread, out var code);
            CloseHandle(thread);
            if (code == 0) throw new InvalidOperationException(LocalizationService.Get("L10n.ErrorDllInjection"));
        }
        finally
        {
            VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        }
    }

    private static (IntPtr init, IntPtr set) ResolveExports(IntPtr process, uint pid, string dllPath, string dllName)
    {
        var local = LoadLibrary(dllPath);
        if (local == IntPtr.Zero) throw new InvalidOperationException(LocalizationService.Format(
            "L10n.ErrorLoadLocalDllTemplate", Marshal.GetLastWin32Error()));
        try
        {
            var initLocal = GetProcAddress(local, "Speedhack_InitThread");
            var setLocal = GetProcAddress(local, "Speedhack_SetSpeedThread");
            if (initLocal == IntPtr.Zero || setLocal == IntPtr.Zero)
                throw new InvalidOperationException(LocalizationService.Get("L10n.ErrorMissingExports"));

            var localBase = local.ToInt64();
            var initOff = initLocal.ToInt64() - localBase;
            var setOff = setLocal.ToInt64() - localBase;
            var remoteBase = FindRemoteModuleBase(pid, dllName)
                ?? throw new InvalidOperationException(LocalizationService.Get("L10n.ErrorRemoteModuleMissing"));
            return (new IntPtr(remoteBase + initOff), new IntPtr(remoteBase + setOff));
        }
        finally
        {
            // keep loaded ok
        }
    }

    private static long? FindRemoteModuleBase(uint pid, string dllName)
    {
        var snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snap == InvalidHandle) return null;
        try
        {
            var me = new MODULEENTRY32 { dwSize = (uint)Marshal.SizeOf<MODULEENTRY32>() };
            if (!Module32First(snap, ref me)) return null;
            var target = dllName.ToLowerInvariant();
            do
            {
                var name = me.szModule?.ToLowerInvariant() ?? "";
                if (name == target) return me.modBaseAddr.ToInt64();
            } while (Module32Next(snap, ref me));
            return null;
        }
        finally
        {
            CloseHandle(snap);
        }
    }

    private static void RemoteCall(IntPtr process, IntPtr addr, IntPtr param)
    {
        var thread = CreateRemoteThread(process, IntPtr.Zero, 0, addr, param, 0, out _);
        if (thread == IntPtr.Zero) throw new InvalidOperationException(LocalizationService.Format(
            "L10n.ErrorRemoteCallTemplate", Marshal.GetLastWin32Error()));
        WaitForSingleObject(thread, 0xFFFFFFFF);
        CloseHandle(thread);
    }

    private static void RemoteCallFloat(IntPtr process, IntPtr addr, float value)
    {
        var remote = VirtualAllocEx(process, IntPtr.Zero, 4, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (remote == IntPtr.Zero) throw new InvalidOperationException(LocalizationService.Get("L10n.ErrorAllocateSpeed"));
        try
        {
            var bytes = BitConverter.GetBytes(value);
            if (!WriteProcessMemory(process, remote, bytes, 4, out _))
                throw new InvalidOperationException(LocalizationService.Get("L10n.ErrorWriteSpeed"));
            RemoteCall(process, addr, remote);
        }
        finally
        {
            VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        }
    }

    public void Dispose() => Detach(silent: true);

    #region native
    const uint PROCESS_CREATE_THREAD = 0x0002;
    const uint PROCESS_QUERY_INFORMATION = 0x0400;
    const uint PROCESS_VM_OPERATION = 0x0008;
    const uint PROCESS_VM_WRITE = 0x0020;
    const uint PROCESS_VM_READ = 0x0010;
    const uint MEM_COMMIT = 0x1000;
    const uint MEM_RESERVE = 0x2000;
    const uint MEM_RELEASE = 0x8000;
    const uint PAGE_READWRITE = 0x04;
    const uint TH32CS_SNAPMODULE = 0x00000008;
    const uint TH32CS_SNAPMODULE32 = 0x00000010;
    static readonly IntPtr InvalidHandle = new(-1);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr OpenProcess(uint dwDesiredAccess, bool bInheritHandle, uint dwProcessId);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool CloseHandle(IntPtr hObject);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr VirtualAllocEx(IntPtr hProcess, IntPtr lpAddress, nuint dwSize, uint flAllocationType, uint flProtect);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool VirtualFreeEx(IntPtr hProcess, IntPtr lpAddress, nuint dwSize, uint dwFreeType);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool WriteProcessMemory(IntPtr hProcess, IntPtr lpBaseAddress, byte[] lpBuffer, int nSize, out int lpNumberOfBytesWritten);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern IntPtr GetModuleHandle(string lpModuleName);

    [DllImport("kernel32.dll", CharSet = CharSet.Ansi, SetLastError = true)]
    static extern IntPtr GetProcAddress(IntPtr hModule, string procName);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern IntPtr LoadLibrary(string lpFileName);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr CreateRemoteThread(IntPtr hProcess, IntPtr lpThreadAttributes, nuint dwStackSize,
        IntPtr lpStartAddress, IntPtr lpParameter, uint dwCreationFlags, out uint lpThreadId);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern uint WaitForSingleObject(IntPtr hHandle, uint dwMilliseconds);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool GetExitCodeThread(IntPtr hThread, out uint lpExitCode);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr CreateToolhelp32Snapshot(uint dwFlags, uint th32ProcessID);

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    struct MODULEENTRY32
    {
        public uint dwSize;
        public uint th32ModuleID;
        public uint th32ProcessID;
        public uint GlblcntUsage;
        public uint ProccntUsage;
        public IntPtr modBaseAddr;
        public uint modBaseSize;
        public IntPtr hModule;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
        public string szModule;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
        public string szExePath;
    }

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern bool Module32First(IntPtr hSnapshot, ref MODULEENTRY32 lpme);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern bool Module32Next(IntPtr hSnapshot, ref MODULEENTRY32 lpme);
    #endregion
}
