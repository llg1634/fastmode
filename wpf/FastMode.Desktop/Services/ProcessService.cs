using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using FastMode.Models;

namespace FastMode.Services;

/// <summary>
/// CE-style application list: visible top-level windows with titles + icons.
/// </summary>
public static class ProcessService
{
    private const uint GW_OWNER = 4;
    private const int GWL_EXSTYLE = -20;
    private const int GWL_STYLE = -16;
    private const long WS_EX_TOOLWINDOW = 0x00000080;
    private const long WS_EX_APPWINDOW = 0x00040000;
    private const long WS_CHILD = 0x40000000;
    private const uint WM_GETICON = 0x007F;
    private const int ICON_SMALL = 0;
    private const int ICON_SMALL2 = 2;
    private const int GCL_HICON = -14;
    private const int GCL_HICONSM = -34;

    private delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    public static List<ProcessItem> List(string? query = null)
    {
        var q = (query ?? "").Trim().ToLowerInvariant();
        var selfPid = (uint)Environment.ProcessId;
        var map = new Dictionary<uint, WindowAcc>();

        EnumWindows((hWnd, _) =>
        {
            try
            {
                if (!IsWindowVisible(hWnd)) return true;
                if (GetWindow(hWnd, GW_OWNER) != IntPtr.Zero) return true;
                if (!IsAltTabWindow(hWnd)) return true;

                var title = GetTitle(hWnd);
                if (string.IsNullOrWhiteSpace(title)) return true;

                GetWindowThreadProcessId(hWnd, out var pid);
                if (pid == 0 || pid == selfPid) return true;

                if (!map.TryGetValue(pid, out var acc))
                {
                    map[pid] = new WindowAcc { Pid = pid, Title = title, Hwnd = hWnd };
                }
                else if (title.Length > acc.Title.Length)
                {
                    acc.Title = title;
                    acc.Hwnd = hWnd;
                }
            }
            catch { /* skip bad hwnd */ }
            return true;
        }, IntPtr.Zero);

        var list = new List<ProcessItem>();
        foreach (var acc in map.Values)
        {
            string name;
            string path;
            string arch;
            try
            {
                using var p = Process.GetProcessById((int)acc.Pid);
                name = string.IsNullOrWhiteSpace(p.ProcessName) ? $"pid-{acc.Pid}" : p.ProcessName + ".exe";
                path = SafePath(p);
                arch = DetectArch(p);
            }
            catch
            {
                name = $"pid-{acc.Pid}.exe";
                path = "";
                arch = "unknown";
            }

            if (!string.IsNullOrEmpty(q))
            {
                var hay = $"{acc.Title} {name} {acc.Pid}".ToLowerInvariant();
                if (!hay.Contains(q)) continue;
            }

            list.Add(new ProcessItem
            {
                Pid = acc.Pid,
                Name = name,
                Title = acc.Title,
                Arch = arch,
                Path = path,
                Icon = TryLoadIcon(path, acc.Hwnd)
            });
        }

        return list
            .OrderBy(x => x.Title, StringComparer.CurrentCultureIgnoreCase)
            .ThenBy(x => x.Name, StringComparer.OrdinalIgnoreCase)
            .ToList();
    }

    private sealed class WindowAcc
    {
        public uint Pid;
        public string Title = "";
        public IntPtr Hwnd;
    }

    private static bool IsAltTabWindow(IntPtr hWnd)
    {
        var ex = GetWindowLongPtr(hWnd, GWL_EXSTYLE).ToInt64();
        if ((ex & WS_EX_TOOLWINDOW) != 0 && (ex & WS_EX_APPWINDOW) == 0)
            return false;
        var style = GetWindowLongPtr(hWnd, GWL_STYLE).ToInt64();
        if ((style & WS_CHILD) != 0) return false;
        return true;
    }

    private static string GetTitle(IntPtr hWnd)
    {
        var len = GetWindowTextLength(hWnd);
        if (len <= 0) return "";
        var sb = new StringBuilder(len + 1);
        _ = GetWindowText(hWnd, sb, sb.Capacity);
        return sb.ToString().Trim();
    }

    private static string SafePath(Process p)
    {
        try { return p.MainModule?.FileName ?? ""; }
        catch { return ""; }
    }

    private static string DetectArch(Process p)
    {
        try
        {
            if (!Environment.Is64BitOperatingSystem) return "x86";
            if (IsWow64Process(p.Handle, out var wow) && wow) return "x86";
            return "x64";
        }
        catch { return "unknown"; }
    }

    private static ImageSource? TryLoadIcon(string path, IntPtr hwnd)
    {
        IntPtr extracted = IntPtr.Zero;
        try
        {
            if (!string.IsNullOrEmpty(path))
            {
                extracted = ExtractIcon(Process.GetCurrentProcess().Handle, path, 0);
                if (extracted == IntPtr.Zero || extracted == new IntPtr(1) || extracted == new IntPtr(-1))
                    extracted = IntPtr.Zero;
            }

            var use = extracted;
            if (use == IntPtr.Zero)
            {
                use = SendMessage(hwnd, WM_GETICON, (IntPtr)ICON_SMALL2, IntPtr.Zero);
                if (use == IntPtr.Zero) use = SendMessage(hwnd, WM_GETICON, (IntPtr)ICON_SMALL, IntPtr.Zero);
                if (use == IntPtr.Zero) use = GetClassLongPtr(hwnd, GCL_HICONSM);
                if (use == IntPtr.Zero) use = GetClassLongPtr(hwnd, GCL_HICON);
            }
            if (use == IntPtr.Zero) return null;

            var src = Imaging.CreateBitmapSourceFromHIcon(
                use, Int32Rect.Empty, BitmapSizeOptions.FromEmptyOptions());
            src.Freeze();
            return src;
        }
        catch
        {
            return null;
        }
        finally
        {
            if (extracted != IntPtr.Zero)
                DestroyIcon(extracted);
        }
    }

    #region native
    [DllImport("user32.dll")]
    private static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern IntPtr GetWindow(IntPtr hWnd, uint uCmd);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetWindowTextLength(IntPtr hWnd);

    [DllImport("user32.dll", EntryPoint = "GetWindowLongPtrW", SetLastError = true)]
    private static extern IntPtr GetWindowLongPtr64(IntPtr hWnd, int nIndex);

    [DllImport("user32.dll", EntryPoint = "GetWindowLongW", SetLastError = true)]
    private static extern int GetWindowLong32(IntPtr hWnd, int nIndex);

    private static IntPtr GetWindowLongPtr(IntPtr hWnd, int nIndex)
        => IntPtr.Size == 8 ? GetWindowLongPtr64(hWnd, nIndex) : new IntPtr(GetWindowLong32(hWnd, nIndex));

    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    private static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", EntryPoint = "GetClassLongPtrW")]
    private static extern IntPtr GetClassLongPtr64(IntPtr hWnd, int nIndex);

    [DllImport("user32.dll", EntryPoint = "GetClassLongW")]
    private static extern uint GetClassLong32(IntPtr hWnd, int nIndex);

    private static IntPtr GetClassLongPtr(IntPtr hWnd, int nIndex)
        => IntPtr.Size == 8 ? GetClassLongPtr64(hWnd, nIndex) : new IntPtr(unchecked((int)GetClassLong32(hWnd, nIndex)));

    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    private static extern IntPtr ExtractIcon(IntPtr hInst, string lpszExeFileName, int nIconIndex);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool DestroyIcon(IntPtr hIcon);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool IsWow64Process(IntPtr hProcess, out bool wow64Process);
    #endregion
}
