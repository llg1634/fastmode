namespace FastMode.Models;

public sealed class ProcessItem
{
    public uint Pid { get; init; }
    public string Name { get; init; } = "";
    public string Title { get; init; } = "";
    public string Arch { get; init; } = "unknown";
    public string Path { get; init; } = "";
    public System.Windows.Media.ImageSource? Icon { get; init; }

    public string DisplayPrimary =>
        string.IsNullOrWhiteSpace(Title) ? Name : Title;

    public string DisplaySecondary =>
        $"{Name}  ·  PID {Pid}  ·  {Arch}";
}

public sealed class AppSettings
{
    public bool AlwaysOnTop { get; set; }
    public List<float> Presets { get; set; } = new() { 0.5f, 1f, 2f, 3f, 5f };
    public float CurrentSpeed { get; set; } = 2f;
    public bool Enabled { get; set; }
    public List<int> HotkeyButtons { get; set; } = new() { 4, 5 };
    public string? LastProcessName { get; set; }
}

public sealed class AttachInfo
{
    public bool Attached { get; set; }
    public uint? Pid { get; set; }
    public string? Name { get; set; }
    public string? Arch { get; set; }
    public bool Enabled { get; set; }
    public float Speed { get; set; } = 1f;
    public string Message { get; set; } = "未附加";
}
