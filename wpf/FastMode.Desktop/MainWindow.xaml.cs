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
    private bool _recording;
    private List<int> _recorded = new();
    private DispatcherTimer? _searchTimer;
    private Brush? _statusNormalBrush;
    private Brush? _statusErrorBrush;

    public MainWindow()
    {
        InitializeComponent();
        Title = "FastMode";
        Loaded += OnLoaded;
        Closed += (_, _) =>
        {
            _pad.Dispose();
            _speed.Dispose();
        };
    }

    private void OnLoaded(object sender, RoutedEventArgs e)
    {
        _statusNormalBrush = (Brush)FindResource("MutedBrush");
        _statusErrorBrush = (Brush)FindResource("DangerBrush");

        ChkTopmost.IsChecked = _settings.AlwaysOnTop;
        Topmost = _settings.AlwaysOnTop;
        TxtCustomSpeed.Text = _settings.CurrentSpeed.ToString("0.###");
        RebuildPresets();
        RefreshProcesses();
        UpdateHotkeyText();
        UpdateSpeedState();
        SetStatus("就绪", error: false);

        _pad.Updated += OnGamepadUpdated;
        _pad.Start(() => _settings.HotkeyButtons);
    }

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
        {
            ListProcesses.SelectedItem = list.FirstOrDefault(x => x.Pid == selectedPid);
        }
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
                return;
            }
            var enabled = ChkEnable.IsChecked == true;
            var st = _speed.SetSpeed(_settings.CurrentSpeed, enabled);
            _settings.Enabled = enabled;
            SettingsService.Save(_settings);
            UpdateSpeedState();
            SetStatus(st.Message, error: false);
        }
        catch (Exception ex)
        {
            ChkEnable.IsChecked = false;
            SetStatus(ex.Message, error: true);
        }
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
            else
            {
                SetStatus($"已记录倍速 {speed:0.###}x（尚未附加）", error: false);
            }
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

    private void BtnHotkey_Click(object sender, RoutedEventArgs e)
    {
        if (_recording)
        {
            if (_recorded.Count > 0)
            {
                _settings.HotkeyButtons = _recorded.OrderBy(x => x).ToList();
                SettingsService.Save(_settings);
            }
            _recording = false;
            _recorded.Clear();
            SetStatus("快捷键已保存", error: false);
            UpdateHotkeyText();
            return;
        }

        _recording = true;
        _recorded.Clear();
        SetStatus("录制中：按下组合键后再次点「改键」保存；右键恢复默认", error: false);
        UpdateHotkeyText();
    }

    private void BtnSettings_Click(object sender, RoutedEventArgs e)
    {
        var presets = string.Join(", ", _settings.Presets.Select(p => p.ToString("0.###")));
        var input = new PromptWindow(
            "设置",
            "倍速预设（逗号分隔）：",
            presets,
            extra:
            $"当前快捷键：{GamepadService.FormatHotkey(_settings.HotkeyButtons)}\n" +
            "主界面「改键」可录制手柄组合。\n" +
            "FastMode 0.2.0 · WPF 原生")
        {
            Owner = this
        };
        if (input.ShowDialog() == true)
        {
            var vals = input.Value
                .Split(new[] { ',', '，', ' ', ';' }, StringSplitOptions.RemoveEmptyEntries)
                .Select(s => float.TryParse(s, out var f) ? f : -1)
                .Where(f => f > 0)
                .Take(8)
                .ToList();
            if (vals.Count > 0)
            {
                _settings.Presets = vals;
                SettingsService.Save(_settings);
                RebuildPresets();
                SetStatus("预设已更新", error: false);
            }
        }
    }

    protected override void OnMouseRightButtonUp(MouseButtonEventArgs e)
    {
        if (_recording)
        {
            _settings.HotkeyButtons = new List<int> { 4, 5 };
            SettingsService.Save(_settings);
            _recording = false;
            _recorded.Clear();
            UpdateHotkeyText();
            SetStatus("已恢复默认 LB + RB", error: false);
            e.Handled = true;
            return;
        }
        base.OnMouseRightButtonUp(e);
    }

    private void OnGamepadUpdated(GamepadStatus status, bool edge)
    {
        Dispatcher.Invoke(() =>
        {
            TxtGamepad.Text = status.Connected
                ? (status.Name ?? "手柄已连接")
                : "手柄未连接";

            if (_recording)
            {
                if (status.ButtonsPressed.Count > 0)
                {
                    _recorded = status.ButtonsPressed.Distinct().OrderBy(x => x).ToList();
                    UpdateHotkeyText();
                }
                return;
            }

            // keep hotkey summary visible while connected/disconnected
            if (!_recording) UpdateHotkeyText();

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
        if (_recording)
        {
            TxtHotkey.Text = _recorded.Count == 0
                ? "录制中…"
                : GamepadService.FormatHotkey(_recorded) + " · 再点保存";
            BtnHotkey.Content = "保存";
        }
        else
        {
            TxtHotkey.Text = GamepadService.FormatHotkey(_settings.HotkeyButtons);
            BtnHotkey.Content = "改键";
        }
    }

    private void UpdateSpeedState()
    {
        var sp = _settings.CurrentSpeed;
        var en = _settings.Enabled && _speed.State.Attached;
        TxtSpeedState.Text = $"当前：{sp:0.###}x · {(en ? "已启用" : "未启用")}";
    }

    /// <summary>
    /// Fixed bottom status only — never inserts banners that reflow the page.
    /// </summary>
    private void SetStatus(string msg, bool error)
    {
        TxtStatus.Text = msg;
        TxtStatus.Foreground = error
            ? (_statusErrorBrush ?? Brushes.IndianRed)
            : (_statusNormalBrush ?? Brushes.Gray);
    }
}
