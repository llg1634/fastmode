using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using FastMode.Models;

namespace FastMode.Services;

public static class ProcessService
{
    public static List<ProcessItem> List(string? query = null)
    {
        var q = (query ?? "").Trim().ToLowerInvariant();
        var list = new List<ProcessItem>();
        foreach (var p in Process.GetProcesses())
        {
            try
            {
                var name = string.IsNullOrWhiteSpace(p.ProcessName) ? "" : p.ProcessName + ".exe";
                if (string.IsNullOrEmpty(name)) continue;
                if (!string.IsNullOrEmpty(q) && !name.ToLowerInvariant().Contains(q)) continue;
                list.Add(new ProcessItem
                {
                    Pid = (uint)p.Id,
                    Name = name,
                    Arch = DetectArch(p)
                });
            }
            catch
            {
                // ignore inaccessible
            }
            finally
            {
                p.Dispose();
            }
        }
        return list.OrderBy(x => x.Name, StringComparer.OrdinalIgnoreCase).ToList();
    }

    private static string DetectArch(Process p)
    {
        try
        {
            if (!Environment.Is64BitOperatingSystem) return "x86";
            if (IsWow64(p.Handle)) return "x86";
            return "x64";
        }
        catch
        {
            return "unknown";
        }
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool IsWow64Process(IntPtr hProcess, out bool wow64);

    private static bool IsWow64(IntPtr handle)
    {
        if (!IsWow64Process(handle, out var wow)) return false;
        return wow;
    }
}
