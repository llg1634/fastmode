using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Threading;
using FastMode.Models;
using FastMode.Services;

namespace FastMode;

public partial class MainWindow : Window
{
    private readonly SpeedhackService _speed = new();
    private readonly GamepadService _pad = new();
    private readonly KeyboardHotkeyService _keyboard = new();
    private AppSettings _settings = SettingsService.Load();
    private DispatcherTimer? _searchTimer;
    private Brush? _statusNormalBrush;
    private Brush? _statusErrorBrush;
    private OverlayWindow? _overlay;
    private GamepadStatus _lastGamepadStatus = new();
    private bool _suppressEnableEvent;

    public MainWindow()
    {
        InitializeComponent();
        Title = "FastMode";
        Loaded += OnLoaded;
        Closed += (_, _) =>
        {
            _pad.Dispose();
            _keyboard.Dispose();
            _speed.Dispose();
            CloseOverlay();
        };
    }

    private void OnLoaded(object sender, RoutedEventArgs e)
    {
        _statusNormalBrush = (Brush)FindResource("ColorBrushGray3");
        _statusErrorBrush = (Brush)FindResource("ColorBrushRedDark");

        ChkTopmost.IsChecked = _settings.AlwaysOnTop;
        Topmost = _settings.AlwaysOnTop;
        ChkEnable.Foreground = (Brush)FindResource("ColorBrush1");
        ChkFloating.Foreground = (Brush)FindResource("ColorBrush1");
        ChkFloating.IsChecked = _settings.FloatingEnabled;

        TxtCustomSpeed.Text = _settings.CurrentSpeed.ToString("0.###");
        RebuildPresets();
        RefreshProcesses();
        UpdateHotkeyText();
        UpdateSpeedState();
        SetStatus(LocalizationService.Get("L10n.Ready"), error: false);
        ApplyOverlayState();

        _pad.Updated += OnGamepadUpdated;
        _pad.Start(() => _settings.HotkeyButtons);
        _keyboard.Triggered += OnKeyboardTriggered;
        try
        {
            _keyboard.Start(() => (_settings.KeyboardModifiers, _settings.KeyboardKey));
        }
        catch (Exception ex)
        {
            SetStatus(ex.Message, error: true);
        }
    }

    #region window chrome
    private void TitleBar_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (e.ClickCount == 2) return;
        if (e.ChangedButton == MouseButton.Left) DragMove();
    }

    private void BtnMin_Click(object sender, RoutedEventArgs e) => WindowState = WindowState.Minimized;
    private void BtnClose_Click(object sender, RoutedEventArgs e) => Close();

    private void BtnLanguage_Click(object sender, RoutedEventArgs e)
    {
        LocalizationService.Toggle();
        UpdateLocalizedStateTexts();
        SetStatus(LocalizationService.Get("L10n.LanguageChanged"), error: false);
    }

    private void Resize_MouseDown(object sender, MouseButtonEventArgs e)
    {
        if (sender is not FrameworkElement fe || fe.Tag is not string dir) return;
        if (e.LeftButton != MouseButtonState.Pressed) return;
        var mode = dir switch
        {
            "N" => 3, "S" => 6, "W" => 1, "E" => 2,
            "NW" => 4, "NE" => 5, "SW" => 7, "SE" => 8,
            _ => 8
        };
        SendMessage(new System.Windows.Interop.WindowInteropHelper(this).Handle, 0x112, (IntPtr)(0xF000 + mode), IntPtr.Zero);
        e.Handled = true;
    }

    [DllImport("user32.dll")]
    private static extern IntPtr SendMessage(IntPtr hWnd, int msg, IntPtr wParam, IntPtr lParam);
    #endregion

    private void RebuildPresets()
    {
        PanelPresets.Children.Clear();
        foreach (var p in _settings.Presets)
        {
            var btn = new ToggleButton
            {
                Content = $"{p:0.###}x",
                Style = (Style)FindResource("ChipButton"),
                Tag = p,
                IsChecked = Math.Abs(_settings.CurrentSpeed - p) < 1e-6
            };
            btn.Click += (_, _) =>
            {
                foreach (ToggleButton t in PanelPresets.Children) t.IsChecked = false;
                btn.IsChecked = true;
                ApplySpeed(p);
            };
            PanelPresets.Children.Add(btn);
        }
    }

    private void RefreshProcesses()
    {
        var q = TxtSearch.Text?.Trim() ?? "";
        var selectedPid = (ListProcesses.SelectedItem as ProcessItem)?.Pid;
        var list = ProcessService.List(q);
        ListProcesses.ItemsSource = list;
        if (selectedPid != null)
            ListProcesses.SelectedItem = list.FirstOrDefault(x => x.Pid == selectedPid);
    }

    private void TxtSearch_TextChanged(object sender, TextChangedEventArgs e)
    {
        _searchTimer ??= new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(200) };
        _searchTimer.Stop();
        _searchTimer.Tick -= SearchTick;
        _searchTimer.Tick += SearchTick;
        _searchTimer.Start();
    }

    private void SearchTick(object? sender, EventArgs e)
    {
        _searchTimer?.Stop();
        RefreshProcesses();
    }

    private void BtnRefresh_Click(object sender, RoutedEventArgs e) => RefreshProcesses();
    private void ListProcesses_MouseDoubleClick(object sender, MouseButtonEventArgs e) => DoAttach();
    private void BtnAttach_Click(object sender, RoutedEventArgs e) => DoAttach();

    private void DoAttach()
    {
        if (ListProcesses.SelectedItem is not ProcessItem item)
        {
            SetStatus(LocalizationService.Get("L10n.SelectProcess"), error: true);
            return;
        }
        try
        {
            var st = _speed.Attach(item);
            _settings.LastProcessName = item.Name;
            _settings.Enabled = false;
            SettingsService.Save(_settings);
            TxtAttach.Text = st.Message;
            SetEnableCheck(false);
            UpdateSpeedState();
            SyncOverlay();
            SetStatus(st.Message, error: false);
        }
        catch (Exception ex)
        {
            SetStatus(ex.Message, error: true);
        }
    }

    private void BtnDetach_Click(object sender, RoutedEventArgs e)
    {
        var st = _speed.Detach();
        _settings.Enabled = false;
        SettingsService.Save(_settings);
        SetEnableCheck(false);
        TxtAttach.Text = st.Message;
        UpdateSpeedState();
        SyncOverlay();
        SetStatus(st.Message, error: false);
    }

    private void ChkEnable_Changed(object sender, RoutedEventArgs e)
    {
        if (!IsLoaded || _suppressEnableEvent) return;
        try
        {
            if (!_speed.State.Attached)
            {
                SetEnableCheck(false);
                SetStatus(LocalizationService.Get("L10n.AttachTargetFirst"), error: true);
                SyncOverlay();
                return;
            }
            SetAccelerationEnabled(ChkEnable.IsChecked == true);
        }
        catch (Exception ex)
        {
            SetEnableCheck(false);
            SyncOverlay();
            SetStatus(ex.Message, error: true);
        }
    }

    private void ChkFloating_Changed(object sender, RoutedEventArgs e)
    {
        if (!IsLoaded) return;
        _settings.FloatingEnabled = ChkFloating.IsChecked == true;
        SettingsService.Save(_settings);
        ApplyOverlayState();
        SetStatus(LocalizationService.Get(
            _settings.FloatingEnabled ? "L10n.FloatingEnabled" : "L10n.FloatingDisabled"), error: false);
    }

    private void ApplyOverlayState()
    {
        if (_settings.FloatingEnabled)
        {
            if (_overlay == null)
            {
                _overlay = new OverlayWindow();
                _overlay.Show();
            }
            SyncOverlay();
        }
        else CloseOverlay();
    }

    private void SyncOverlay()
    {
        var on = ChkEnable.IsChecked == true && _speed.State.Attached;
        _overlay?.SetEnabledState(on);
    }

    private void CloseOverlay()
    {
        if (_overlay == null) return;
        try { _overlay.Close(); } catch { /* ignore */ }
        _overlay = null;
    }

    private void BtnApplySpeed_Click(object sender, RoutedEventArgs e)
    {
        if (!float.TryParse(TxtCustomSpeed.Text, out var speed))
        {
            SetStatus(LocalizationService.Get("L10n.SpeedMustBeNumber"), error: true);
            return;
        }
        ApplySpeed(speed);
    }

    private void ApplySpeed(float speed)
    {
        try
        {
            _settings.CurrentSpeed = speed;
            TxtCustomSpeed.Text = speed.ToString("0.###");
            SettingsService.Save(_settings);
            if (_speed.State.Attached)
            {
                var st = _speed.SetSpeed(speed, _settings.Enabled);
                SetStatus(st.Message, error: false);
            }
            else SetStatus(LocalizationService.Format("L10n.SavedSpeedTemplate", speed), error: false);
            RebuildPresets();
            UpdateSpeedState();
        }
        catch (Exception ex)
        {
            SetStatus(ex.Message, error: true);
        }
    }

    private void ChkTopmost_Changed(object sender, RoutedEventArgs e)
    {
        if (!IsLoaded) return;
        Topmost = ChkTopmost.IsChecked == true;
        _settings.AlwaysOnTop = Topmost;
        SettingsService.Save(_settings);
    }

    private void BtnSettings_Click(object sender, RoutedEventArgs e)
    {
        var win = new SettingsWindow(
            _settings.HotkeyButtons,
            _settings.KeyboardModifiers,
            _settings.KeyboardKey) { Owner = this };
        if (win.ShowDialog() == true)
        {
            var oldKeyboard = KeyboardHotkeyService.FormatHotkey(_settings.KeyboardModifiers, _settings.KeyboardKey);
            _settings.HotkeyButtons = win.ResultHotkeys;
            _settings.KeyboardModifiers = win.ResultKeyboardModifiers;
            _settings.KeyboardKey = win.ResultKeyboardKey;
            SettingsService.Save(_settings);
            UpdateHotkeyText();
            var newGamepad = GamepadService.FormatHotkey(_settings.HotkeyButtons);
            var newKeyboard = KeyboardHotkeyService.FormatHotkey(_settings.KeyboardModifiers, _settings.KeyboardKey);
            var key = oldKeyboard != newKeyboard
                ? "L10n.KeyboardShortcutUpdatedTemplate"
                : "L10n.ControllerShortcutUpdatedTemplate";
            SetStatus(LocalizationService.Format(key, oldKeyboard != newKeyboard ? newKeyboard : newGamepad), error: false);
        }
    }

    private void OnGamepadUpdated(GamepadStatus status, bool edge)
    {
        Dispatcher.Invoke(() =>
        {
            _lastGamepadStatus = status;
            UpdateGamepadStatus();
            UpdateHotkeyText();
            if (!edge) return;
            ToggleAcceleration("L10n.ControllerTogglePrefix");
        });
    }

    private void OnKeyboardTriggered() => ToggleAcceleration("L10n.KeyboardTogglePrefix");

    private void UpdateHotkeyText()
    {
        TxtHotkey.Text = GamepadService.FormatHotkey(_settings.HotkeyButtons);
    }

    private void UpdateSpeedState()
    {
        var sp = _settings.CurrentSpeed;
        var en = _settings.Enabled && _speed.State.Attached;
        TxtSpeedState.Text = LocalizationService.Format(
            "L10n.CurrentSpeedTemplate",
            sp,
            LocalizationService.Get(en ? "L10n.EnabledState" : "L10n.DisabledState"));
    }

    private void UpdateGamepadStatus()
    {
        TxtGamepad.Text = _lastGamepadStatus.Connected
            ? (_lastGamepadStatus.Name ?? LocalizationService.Get("L10n.GamepadConnected"))
            : LocalizationService.Get("L10n.GamepadDisconnected");
    }

    private void UpdateLocalizedStateTexts()
    {
        UpdateGamepadStatus();
        UpdateHotkeyText();
        UpdateSpeedState();
        TxtAttach.Text = _speed.State.Attached
            ? LocalizationService.Format("L10n.AttachedTemplate", _speed.State.Name ?? "", _speed.State.Arch ?? "")
            : LocalizationService.Get("L10n.NotAttached");
    }

    private void ToggleAcceleration(string sourceKey)
    {
        if (!_speed.State.Attached)
        {
            SetStatus(LocalizationService.Get("L10n.AttachTargetFirst"), error: true);
            return;
        }

        try
        {
            SetAccelerationEnabled(!_settings.Enabled, sourceKey);
        }
        catch (Exception ex)
        {
            SetEnableCheck(false);
            SyncOverlay();
            SetStatus(ex.Message, error: true);
        }
    }

    private void SetAccelerationEnabled(bool enabled, string? sourceKey = null)
    {
        var state = _speed.SetSpeed(_settings.CurrentSpeed, enabled);
        _settings.Enabled = enabled;
        SettingsService.Save(_settings);
        SetEnableCheck(enabled);
        UpdateSpeedState();
        SyncOverlay();
        var message = sourceKey == null
            ? state.Message
            : LocalizationService.Get(sourceKey) + " · " + state.Message;
        SetStatus(message, error: false);
    }

    private void SetEnableCheck(bool enabled)
    {
        _suppressEnableEvent = true;
        ChkEnable.IsChecked = enabled;
        _suppressEnableEvent = false;
    }

    private void SetStatus(string msg, bool error)
    {
        TxtStatus.Text = msg;
        TxtStatus.Foreground = error
            ? (_statusErrorBrush ?? Brushes.IndianRed)
            : (_statusNormalBrush ?? Brushes.Gray);
    }
}
