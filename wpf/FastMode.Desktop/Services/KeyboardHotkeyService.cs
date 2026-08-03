using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Windows;
using FastMode.Models;

namespace FastMode.Services;

public sealed class KeyboardHotkeyService : IDisposable
{
    private const int WhKeyboardLl = 13;
    private const int WmKeyDown = 0x0100;
    private const int WmKeyUp = 0x0101;
    private const int WmSysKeyDown = 0x0104;
    private const int WmSysKeyUp = 0x0105;

    private readonly HookProc _callback;
    private Func<(ShortcutModifiers Modifiers, int Key)>? _getShortcut;
    private IntPtr _hook = IntPtr.Zero;
    private int _pressedKey;

    public KeyboardHotkeyService()
    {
        _callback = HookCallback;
    }

    public event Action? Triggered;

    public void Start(Func<(ShortcutModifiers Modifiers, int Key)> getShortcut)
    {
        Stop();
        _getShortcut = getShortcut;
        using var process = Process.GetCurrentProcess();
        using var module = process.MainModule;
        var moduleHandle = GetModuleHandle(module?.ModuleName);
        _hook = SetWindowsHookEx(WhKeyboardLl, _callback, moduleHandle, 0);
        if (_hook == IntPtr.Zero)
            throw new InvalidOperationException("SetWindowsHookEx failed: " + Marshal.GetLastWin32Error());
    }

    public void Stop()
    {
        if (_hook != IntPtr.Zero)
        {
            UnhookWindowsHookEx(_hook);
            _hook = IntPtr.Zero;
        }
        _pressedKey = 0;
    }

    private IntPtr HookCallback(int code, IntPtr wParam, IntPtr lParam)
    {
        if (code >= 0 && _getShortcut != null)
        {
            var message = wParam.ToInt32();
            var data = Marshal.PtrToStructure<KbdLlHookStruct>(lParam);
            var shortcut = _getShortcut();

            if ((message == WmKeyDown || message == WmSysKeyDown) &&
                data.VirtualKey == shortcut.Key &&
                _pressedKey == 0 &&
                ModifiersMatch(shortcut.Modifiers))
            {
                _pressedKey = shortcut.Key;
                Application.Current.Dispatcher.BeginInvoke(() => Triggered?.Invoke());
            }
            else if ((message == WmKeyUp || message == WmSysKeyUp) && data.VirtualKey == _pressedKey)
            {
                _pressedKey = 0;
            }
        }

        return CallNextHookEx(_hook, code, wParam, lParam);
    }

    private static bool ModifiersMatch(ShortcutModifiers required)
    {
        var control = IsDown(0x11);
        var shift = IsDown(0x10);
        var alt = IsDown(0x12);
        return control == required.HasFlag(ShortcutModifiers.Control) &&
               shift == required.HasFlag(ShortcutModifiers.Shift) &&
               alt == required.HasFlag(ShortcutModifiers.Alt);
    }

    private static bool IsDown(int virtualKey) => (GetAsyncKeyState(virtualKey) & 0x8000) != 0;

    public static bool IsSupportedKey(int virtualKey) =>
        virtualKey is >= 0x30 and <= 0x39 or >= 0x41 and <= 0x5A or >= 0x70 and <= 0x7B;

    public static string FormatHotkey(ShortcutModifiers modifiers, int virtualKey)
    {
        var parts = new List<string>();
        if (modifiers.HasFlag(ShortcutModifiers.Control)) parts.Add("Ctrl");
        if (modifiers.HasFlag(ShortcutModifiers.Shift)) parts.Add("Shift");
        if (modifiers.HasFlag(ShortcutModifiers.Alt)) parts.Add("Alt");
        if (IsSupportedKey(virtualKey)) parts.Add(KeyName(virtualKey));
        return parts.Count == 0 ? LocalizationService.Get("L10n.NotSet") : string.Join(" + ", parts);
    }

    public static string KeyName(int virtualKey) => virtualKey switch
    {
        >= 0x30 and <= 0x39 => ((char)virtualKey).ToString(),
        >= 0x41 and <= 0x5A => ((char)virtualKey).ToString(),
        >= 0x70 and <= 0x7B => "F" + (virtualKey - 0x6F),
        _ => "0x" + virtualKey.ToString("X2")
    };

    public void Dispose() => Stop();

    private delegate IntPtr HookProc(int code, IntPtr wParam, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    private struct KbdLlHookStruct
    {
        public int VirtualKey;
        public int ScanCode;
        public int Flags;
        public int Time;
        public IntPtr ExtraInfo;
    }

    [DllImport("user32.dll", SetLastError = true)]
    private static extern IntPtr SetWindowsHookEx(int idHook, HookProc callback, IntPtr module, uint threadId);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool UnhookWindowsHookEx(IntPtr hook);

    [DllImport("user32.dll")]
    private static extern IntPtr CallNextHookEx(IntPtr hook, int code, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern short GetAsyncKeyState(int virtualKey);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    private static extern IntPtr GetModuleHandle(string? moduleName);
}
