using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using FastMode.Services;

namespace FastMode;

public partial class SettingsWindow : Window
{
    private readonly HashSet<int> _selected;
    private readonly List<Button> _padButtons = new();

    public List<int> ResultHotkeys { get; private set; }

    public SettingsWindow(IEnumerable<int> current)
    {
        InitializeComponent();
        _selected = current.ToHashSet();
        ResultHotkeys = _selected.OrderBy(x => x).ToList();
        Loaded += (_, _) =>
        {
            CollectPadButtons(this);
            RefreshPadVisual();
            RefreshSelectedText();
        };
    }

    private void CollectPadButtons(DependencyObject root)
    {
        var count = VisualTreeHelper.GetChildrenCount(root);
        for (var i = 0; i < count; i++)
        {
            var child = VisualTreeHelper.GetChild(root, i);
            if (child is Button b && b.Tag != null && int.TryParse(b.Tag.ToString(), out _))
                _padButtons.Add(b);
            CollectPadButtons(child);
        }
    }

    private void Title_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (e.LeftButton == MouseButtonState.Pressed) DragMove();
    }

    private void BtnClose_Click(object sender, RoutedEventArgs e)
    {
        DialogResult = false;
        Close();
    }

    private void Nav_Checked(object sender, RoutedEventArgs e)
    {
        if (!IsLoaded) return;
        if (sender is not RadioButton rb) return;
        var tag = rb.Tag?.ToString();
        PageHotkey.Visibility = tag == "hotkey" ? Visibility.Visible : Visibility.Collapsed;
        PageAbout.Visibility = tag == "about" ? Visibility.Visible : Visibility.Collapsed;
    }

    private void PadBtn_Click(object sender, RoutedEventArgs e)
    {
        if (sender is not Button b || b.Tag is null || !int.TryParse(b.Tag.ToString(), out var id)) return;
        if (!_selected.Add(id)) _selected.Remove(id);
        RefreshPadVisual();
        RefreshSelectedText();
    }

    private void BtnClear_Click(object sender, RoutedEventArgs e)
    {
        _selected.Clear();
        RefreshPadVisual();
        RefreshSelectedText();
    }

    private void BtnDefault_Click(object sender, RoutedEventArgs e)
    {
        _selected.Clear();
        _selected.Add(4);
        _selected.Add(5);
        RefreshPadVisual();
        RefreshSelectedText();
    }

    private void BtnSave_Click(object sender, RoutedEventArgs e)
    {
        if (_selected.Count == 0)
        {
            MessageBox.Show(this, "请至少选择一个按键。", "FastMode", MessageBoxButton.OK, MessageBoxImage.Information);
            return;
        }
        ResultHotkeys = _selected.OrderBy(x => x).ToList();
        DialogResult = true;
        Close();
    }

    private void RefreshSelectedText()
    {
        TxtSelected.Text = _selected.Count == 0
            ? "已选：无"
            : "已选：" + GamepadService.FormatHotkey(_selected.OrderBy(x => x));
    }

    private void RefreshPadVisual()
    {
        var onBg = (Brush)FindResource("ColorBrush3");
        var onBorder = (Brush)FindResource("ColorBrush2");
        var offBg = Brushes.White;
        var offBorder = new SolidColorBrush(Color.FromRgb(0xB7, 0xD9, 0xC6));
        foreach (var b in _padButtons)
        {
            if (!int.TryParse(b.Tag?.ToString(), out var id)) continue;
            var on = _selected.Contains(id);
            b.Background = on ? onBg : offBg;
            b.BorderBrush = on ? onBorder : offBorder;
            b.Foreground = on ? Brushes.White : (Brush)FindResource("ColorBrush1");
        }
    }
}
