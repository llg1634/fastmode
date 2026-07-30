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
    private AppSettings _settings = SettingsService.Load();
    private DispatcherTimer? _searchTimer;
    private Brush? _statusNormalBrush;
    private Brush? _statusErrorBrush;
    private OverlayWindow? _overlay;

    public MainWindow()
    {
        InitializeComponent();
        Title = "FastMode";
        Loaded += OnLoaded;
        Closed += (_, _) =>
        {
            _pad.Dispose();
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
        SetStatus("就绪", error: false);
        ApplyOverlayState();

        _pad.Updated += OnGamepadUpdated;
        _pad.Start(() => _settings.HotkeyButtons);
    }

    #region window chrome
    private void TitleBar_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (e.ClickCount == 2) return;
        if (e.ChangedButton == MouseButton.Left) DragMove();
    }

    private void BtnMin_Click(object sender, RoutedEventArgs e) => WindowState = WindowState.Minimized;
    private void BtnClose_Click(object sender, RoutedEventArgs e) => Close();

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
            SetStatus("请先选择一个进程", error: true);
            return;
        }
        try
        {
            var st = _speed.Attach(item);
            _settings.LastProcessName = item.Name;
            _settings.Enabled = false;
            SettingsService.Save(_settings);
            TxtAttach.Text = st.Message;
            ChkEnable.IsChecked = false;
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
        ChkEnable.IsChecked = false;
        TxtAttach.Text = st.Message;
        UpdateSpeedState();
        SyncOverlay();
        SetStatus(st.Message, error: false);
    }

    private void ChkEnable_Changed(object sender, RoutedEventArgs e)
    {
        if (!IsLoaded) return;
        try
        {
            if (!_speed.State.Attached)
            {
                ChkEnable.IsChecked = false;
                SetStatus("请先附加目标进程", error: true);
                SyncOverlay();
                return;
            }
            var enabled = ChkEnable.IsChecked == true;
            var st = _speed.SetSpeed(_settings.CurrentSpeed, enabled);
            _settings.Enabled = enabled;
            SettingsService.Save(_settings);
            UpdateSpeedState();
            SyncOverlay();
            SetStatus(st.Message, error: false);
        }
        catch (Exception ex)
        {
            ChkEnable.IsChecked = false;
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
        SetStatus(_settings.FloatingEnabled ? "悬浮窗已开启" : "悬浮窗已关闭", error: false);
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
        _overlay?.SetEnabledState(_settings.Enabled && _speed.State.Attached && _speed.State.Enabled);
        // use settings.Enabled after attach semantics: show On only when speedhack actively enabled
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
            SetStatus("倍速必须是数字", error: true);
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
            else SetStatus($"已记录倍速 {speed:0.###}x（尚未附加）", error: false);
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
        var win = new SettingsWindow(_settings.HotkeyButtons) { Owner = this };
        if (win.ShowDialog() == true)
        {
            _settings.HotkeyButtons = win.ResultHotkeys;
            SettingsService.Save(_settings);
            UpdateHotkeyText();
            SetStatus("手柄快捷键已更新：" + GamepadService.FormatHotkey(_settings.HotkeyButtons), error: false);
        }
    }

    private void OnGamepadUpdated(GamepadStatus status, bool edge)
    {
        Dispatcher.Invoke(() =>
        {
            TxtGamepad.Text = status.Connected ? (status.Name ?? "手柄已连接") : "手柄未连接";
            UpdateHotkeyText();
            if (!edge) return;
            if (!_speed.State.Attached)
            {
                SetStatus("请先附加目标进程", error: true);
                return;
            }
            try
            {
                var next = !_settings.Enabled;
                var st = _speed.SetSpeed(_settings.CurrentSpeed, next);
                _settings.Enabled = next;
                SettingsService.Save(_settings);
                ChkEnable.IsChecked = next;
                UpdateSpeedState();
                SyncOverlay();
                SetStatus("手柄切换 · " + st.Message, error: false);
            }
            catch (Exception ex)
            {
                SetStatus(ex.Message, error: true);
            }
        });
    }

    private void UpdateHotkeyText()
    {
        TxtHotkey.Text = GamepadService.FormatHotkey(_settings.HotkeyButtons);
    }

    private void UpdateSpeedState()
    {
        var sp = _settings.CurrentSpeed;
        var en = _settings.Enabled && _speed.State.Attached;
        TxtSpeedState.Text = $"当前：{sp:0.###}x · {(en ? "已启用" : "未启用")}";
    }

    private void SetStatus(string msg, bool error)
    {
        TxtStatus.Text = msg;
        TxtStatus.Foreground = error
            ? (_statusErrorBrush ?? Brushes.IndianRed)
            : (_statusNormalBrush ?? Brushes.Gray);
    }
}
