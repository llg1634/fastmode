using System.Globalization;
using System.Windows;

namespace FastMode.Services;

public enum AppLanguage
{
    Chinese,
    English
}

public static class LocalizationService
{
    private const string ResourcePrefix = "Resources/Strings.";

    public static AppLanguage CurrentLanguage { get; private set; } = AppLanguage.Chinese;

    public static void SetLanguage(AppLanguage language)
    {
        var cultureName = language == AppLanguage.Chinese ? "zh-CN" : "en-US";
        var dictionaries = Application.Current.Resources.MergedDictionaries;
        var replacement = new ResourceDictionary
        {
            Source = new Uri(string.Concat(ResourcePrefix, cultureName, ".xaml"), UriKind.Relative)
        };

        var index = -1;
        for (var i = 0; i < dictionaries.Count; i++)
        {
            if (dictionaries[i].Source?.OriginalString.Contains(ResourcePrefix, StringComparison.OrdinalIgnoreCase) == true)
            {
                index = i;
                break;
            }
        }

        if (index >= 0) dictionaries[index] = replacement;
        else dictionaries.Insert(0, replacement);

        CurrentLanguage = language;
        CultureInfo.CurrentUICulture = CultureInfo.GetCultureInfo(cultureName);
    }

    public static void Toggle() => SetLanguage(
        CurrentLanguage == AppLanguage.Chinese ? AppLanguage.English : AppLanguage.Chinese);

    public static string Get(string key) =>
        Application.Current.TryFindResource(key)?.ToString() ?? key;

    public static string Format(string key, params object[] args) =>
        string.Format(CultureInfo.CurrentCulture, Get(key), args);
}
