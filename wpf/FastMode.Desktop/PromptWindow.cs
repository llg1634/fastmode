using System.Windows.Controls;
using System.Windows;
using FastMode.Services;

namespace FastMode;

public partial class PromptWindow : Window
{
    public string Value => Txt.Text;

    public PromptWindow(string title, string label, string initial, string? extra = null)
    {
        Title = title;
        Width = 420;
        Height = extra == null ? 200 : 320;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        ResizeMode = ResizeMode.NoResize;
        var root = new Grid { Margin = new Thickness(16) };
        root.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        root.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        root.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Star) });
        root.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });

        var lb = new TextBlock { Text = label, Margin = new Thickness(0, 0, 0, 8) };
        Txt = new TextBox { Text = initial, Padding = new Thickness(8), Margin = new Thickness(0, 0, 0, 8) };
        var extraBlock = new TextBlock
        {
            Text = extra ?? "",
            TextWrapping = TextWrapping.Wrap,
            Foreground = (System.Windows.Media.Brush)Application.Current.Resources["MutedBrush"],
            Visibility = extra == null ? Visibility.Collapsed : Visibility.Visible
        };
        var buttons = new StackPanel { Orientation = Orientation.Horizontal, HorizontalAlignment = HorizontalAlignment.Right };
        var ok = new Button { Content = LocalizationService.Get("L10n.Save"), Width = 88, Margin = new Thickness(0, 0, 8, 0), IsDefault = true };
        var cancel = new Button { Content = LocalizationService.Get("L10n.Cancel"), Width = 88, IsCancel = true };
        ok.Click += (_, _) => { DialogResult = true; Close(); };
        cancel.Click += (_, _) => { DialogResult = false; Close(); };
        buttons.Children.Add(ok);
        buttons.Children.Add(cancel);

        Grid.SetRow(lb, 0);
        Grid.SetRow(Txt, 1);
        Grid.SetRow(extraBlock, 2);
        Grid.SetRow(buttons, 3);
        root.Children.Add(lb);
        root.Children.Add(Txt);
        root.Children.Add(extraBlock);
        root.Children.Add(buttons);
        Content = root;
    }

    private TextBox Txt { get; }
}

