using System.Windows;
using FastMode.Services;

namespace FastMode;

public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        LocalizationService.SetLanguage(AppLanguage.Chinese);
    }
}
