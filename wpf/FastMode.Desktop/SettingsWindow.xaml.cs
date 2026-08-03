using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using FastMode.Models;
using FastMode.Services;

namespace FastMode;

public partial class SettingsWindow : Window
{
    private readonly HashSet<int> _selectedGamepad;
    private readonly List<Button> _padButtons = new();
    private readonly List<Button> _keyboardButtons = new();
    private ShortcutModifiers _selectedKeyboardModifiers;
    private int _selectedKeyboardKey;

    public List<int> ResultHotkeys { get; private set; }
    public ShortcutModifiers ResultKeyboardModifiers { get; private set; }
    public int ResultKeyboardKey { get; private set; }

    public SettingsWindow(
        IEnumerable<int> currentGamepad,
        ShortcutModifiers currentKeyboardModifiers,
        int currentKeyboardKey)
    {
        InitializeComponent();
        _selectedGamepad = currentGamepad.ToHashSet();
        _selectedKeyboardModifiers = currentKeyboardModifiers;
        _selectedKeyboardKey = currentKeyboardKey;
        ResultHotkeys = _selectedGamepad.OrderBy(x => x).ToList();
        ResultKeyboardModifiers = _selectedKeyboardModifiers;
        ResultKeyboardKey = _selectedKeyboardKey;

        Loaded += (_, _) =>
        {
            _padButtons.AddRange(PadCanvas.Children.OfType<Button>());
            BuildKeyboardButtons();
            RefreshPadVisual();
            RefreshGamepadSelectedText();
            RefreshKeyboardVisual();
            RefreshKeyboardSelectedText();
        };
    }

    private void BuildKeyboardButtons()
    {
        if (_keyboardButtons.Count != 0) return;

        var keys = Enumerable.Range(0x41, 26)
            .Concat(Enumerable.Range(0x30, 10))
            .Concat(Enumerable.Range(0x70, 12));

        foreach (var virtualKey in keys)
        {
            var button = new Button
            {
                Content = KeyboardHotkeyService.KeyName(virtualKey),
                Tag = virtualKey,
                Style = (Style)FindResource("KeyboardKey")
            };
            button.Click += KeyboardKeyBtn_Click;
            _keyboardButtons.Add(button);
            PanelKeyboardKeys.Children.Add(button);
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
        if (!IsLoaded || sender is not RadioButton radio) return;
        var tag = radio.Tag?.ToString();
        PageGamepad.Visibility = tag == "gamepad" ? Visibility.Visible : Visibility.Collapsed;
        PageKeyboard.Visibility = tag == "keyboard" ? Visibility.Visible : Visibility.Collapsed;
        PageAbout.Visibility = tag == "about" ? Visibility.Visible : Visibility.Collapsed;
    }

    private void PadBtn_Click(object sender, RoutedEventArgs e)
    {
        if (sender is not Button button ||
            button.Tag is null ||
            !int.TryParse(button.Tag.ToString(), out var id))
            return;

        if (!_selectedGamepad.Add(id)) _selectedGamepad.Remove(id);
        RefreshPadVisual();
        RefreshGamepadSelectedText();
    }

    private void BtnPadClear_Click(object sender, RoutedEventArgs e)
    {
        _selectedGamepad.Clear();
        RefreshPadVisual();
        RefreshGamepadSelectedText();
    }

    private void BtnPadDefault_Click(object sender, RoutedEventArgs e)
    {
        _selectedGamepad.Clear();
        _selectedGamepad.Add(4);
        _selectedGamepad.Add(5);
        RefreshPadVisual();
        RefreshGamepadSelectedText();
    }

    private void ModifierBtn_Click(object sender, RoutedEventArgs e)
    {
        if (sender is not Button button ||
            !Enum.TryParse<ShortcutModifiers>(button.Tag?.ToString(), out var modifier))
            return;

        if (_selectedKeyboardModifiers.HasFlag(modifier))
            _selectedKeyboardModifiers &= ~modifier;
        else
            _selectedKeyboardModifiers |= modifier;

        RefreshKeyboardVisual();
        RefreshKeyboardSelectedText();
    }

    private void KeyboardKeyBtn_Click(object sender, RoutedEventArgs e)
    {
        if (sender is not Button button ||
            button.Tag is null ||
            !int.TryParse(button.Tag.ToString(), out var virtualKey))
            return;

        _selectedKeyboardKey = virtualKey;
        RefreshKeyboardVisual();
        RefreshKeyboardSelectedText();
    }

    private void BtnKeyboardDefault_Click(object sender, RoutedEventArgs e)
    {
        _selectedKeyboardModifiers = ShortcutModifiers.Control;
        _selectedKeyboardKey = 0x46;
        RefreshKeyboardVisual();
        RefreshKeyboardSelectedText();
    }

    private void BtnSave_Click(object sender, RoutedEventArgs e)
    {
        if (_selectedGamepad.Count == 0)
        {
            MessageBox.Show(this, LocalizationService.Get("L10n.ControllerKeyRequired"), "FastMode",
                MessageBoxButton.OK, MessageBoxImage.Information);
            NavGamepad.IsChecked = true;
            return;
        }
        if (_selectedKeyboardModifiers == ShortcutModifiers.None)
        {
            MessageBox.Show(this, LocalizationService.Get("L10n.KeyboardModifierRequired"), "FastMode",
                MessageBoxButton.OK, MessageBoxImage.Information);
            NavKeyboard.IsChecked = true;
            return;
        }
        if (!KeyboardHotkeyService.IsSupportedKey(_selectedKeyboardKey))
        {
            MessageBox.Show(this, LocalizationService.Get("L10n.KeyboardMainKeyRequired"), "FastMode",
                MessageBoxButton.OK, MessageBoxImage.Information);
            NavKeyboard.IsChecked = true;
            return;
        }

        ResultHotkeys = _selectedGamepad.OrderBy(x => x).ToList();
        ResultKeyboardModifiers = _selectedKeyboardModifiers;
        ResultKeyboardKey = _selectedKeyboardKey;
        DialogResult = true;
        Close();
    }

    private void RefreshGamepadSelectedText()
    {
        var selection = _selectedGamepad.Count == 0
            ? LocalizationService.Get("L10n.None")
            : GamepadService.FormatHotkey(_selectedGamepad.OrderBy(x => x));
        TxtGamepadSelected.Text = LocalizationService.Format("L10n.SelectedTemplate", selection);
    }

    private void RefreshKeyboardSelectedText()
    {
        TxtKeyboardSelected.Text = LocalizationService.Format(
            "L10n.SelectedTemplate",
            KeyboardHotkeyService.FormatHotkey(_selectedKeyboardModifiers, _selectedKeyboardKey));
    }

    private void RefreshPadVisual()
    {
        foreach (var button in _padButtons)
        {
            if (!int.TryParse(button.Tag?.ToString(), out var id)) continue;
            SetButtonSelected(button, _selectedGamepad.Contains(id));
        }
    }

    private void RefreshKeyboardVisual()
    {
        SetButtonSelected(BtnModifierControl, _selectedKeyboardModifiers.HasFlag(ShortcutModifiers.Control));
        SetButtonSelected(BtnModifierShift, _selectedKeyboardModifiers.HasFlag(ShortcutModifiers.Shift));
        SetButtonSelected(BtnModifierAlt, _selectedKeyboardModifiers.HasFlag(ShortcutModifiers.Alt));
        foreach (var button in _keyboardButtons)
        {
            if (!int.TryParse(button.Tag?.ToString(), out var virtualKey)) continue;
            SetButtonSelected(button, virtualKey == _selectedKeyboardKey);
        }
    }

    private void SetButtonSelected(Button button, bool selected)
    {
        button.Background = selected ? (Brush)FindResource("ColorBrush3") : Brushes.White;
        button.BorderBrush = selected
            ? (Brush)FindResource("ColorBrush2")
            : new SolidColorBrush(Color.FromRgb(0xB7, 0xD9, 0xC6));
        button.Foreground = selected ? Brushes.White : (Brush)FindResource("ColorBrush1");
    }
}
