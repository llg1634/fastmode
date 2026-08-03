using System.IO;
using System.Text.Json;
using FastMode.Models;

namespace FastMode.Services;

public static class SettingsService
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        WriteIndented = true,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase
    };

    private static string PathFile
    {
        get
        {
            var dir = System.IO.Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                "FastMode");
            Directory.CreateDirectory(dir);
            return System.IO.Path.Combine(dir, "settings.json");
        }
    }

    public static AppSettings Load()
    {
        try
        {
            if (!File.Exists(PathFile)) return new AppSettings();
            var json = File.ReadAllText(PathFile);
            var settings = JsonSerializer.Deserialize<AppSettings>(json, JsonOptions) ?? new AppSettings();
            var validModifiers = settings.KeyboardModifiers != ShortcutModifiers.None &&
                                 (settings.KeyboardModifiers & ~(ShortcutModifiers.Control | ShortcutModifiers.Shift | ShortcutModifiers.Alt)) == 0;
            if (!validModifiers) settings.KeyboardModifiers = ShortcutModifiers.Control;
            if (!KeyboardHotkeyService.IsSupportedKey(settings.KeyboardKey)) settings.KeyboardKey = 0x46;
            return settings;
        }
        catch
        {
            return new AppSettings();
        }
    }

    public static void Save(AppSettings settings)
    {
        var json = JsonSerializer.Serialize(settings, JsonOptions);
        File.WriteAllText(PathFile, json);
    }
}
