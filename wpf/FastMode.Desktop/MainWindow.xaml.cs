using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
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
        ChkTopmost.IsChecked = _settings.AlwaysOnTop;
        Topmost = _settings.AlwaysOnTop;
        TxtCustomSpeed.Text = _settings.CurrentSpeed.ToString("0.###");
        TxtSearch.ToolTip = "搜索窗口标题 / 进程名";
        RebuildPresets();
        RefreshProcesses();
        UpdateHotkeyText();
        UpdateSpeedState();

        _pad.Updated += OnGamepadUpdated;
        _pad.Start(() => _settings.HotkeyButtons);
        TxtStatus.Text = "就绪 · 原生 WPF（不依赖本地 Web 端口）";
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
        ListProcesses.ItemsSource = ProcessService.List(q);
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
        ClearError();
        if (ListProcesses.SelectedItem is not ProcessItem item)
        {
            ShowError("请先选择一个进程");
            return;
        }
        try
        {
            var st = _speed.Attach(item);
            _settings.LastProcessName = item.Name;
            _settings.Enabled = false;
            SettingsService.Save(_settings);
            TxtAttach.Text = st.Message;
            TxtStatus.Text = st.Message;
            ChkEnable.IsChecked = false;
            UpdateSpeedState();
        }
        catch (Exception ex)
        {
            ShowError(ex.Message);
        }
    }

    private void BtnDetach_Click(object sender, RoutedEventArgs e)
    {
        ClearError();
        var st = _speed.Detach();
        _settings.Enabled = false;
        SettingsService.Save(_settings);
        ChkEnable.IsChecked = false;
        TxtAttach.Text = st.Message;
        TxtStatus.Text = st.Message;
        UpdateSpeedState();
    }

    private void ChkEnable_Changed(object sender, RoutedEventArgs e)
    {
        if (!IsLoaded) return;
        ClearError();
        try
        {
            if (!_speed.State.Attached)
            {
                ChkEnable.IsChecked = false;
                throw new InvalidOperationException("请先附加目标进程");
            }
            var enabled = ChkEnable.IsChecked == true;
            var speed = _settings.CurrentSpeed;
            var st = _speed.SetSpeed(speed, enabled);
            _settings.Enabled = enabled;
            SettingsService.Save(_settings);
            TxtStatus.Text = st.Message;
            UpdateSpeedState();
        }
        catch (Exception ex)
        {
            ChkEnable.IsChecked = false;
            ShowError(ex.Message);
        }
    }

    private void BtnApplySpeed_Click(object sender, RoutedEventArgs e)
    {
        if (!float.TryParse(TxtCustomSpeed.Text, out var speed))
        {
            ShowError("倍速必须是数字");
            return;
        }
        ApplySpeed(speed);
    }

    private void ApplySpeed(float speed)
    {
        ClearError();
        try
        {
            _settings.CurrentSpeed = speed;
            TxtCustomSpeed.Text = speed.ToString("0.###");
            SettingsService.Save(_settings);
            if (_speed.State.Attached)
            {
                var st = _speed.SetSpeed(speed, _settings.Enabled);
                TxtStatus.Text = st.Message;
            }
            else
            {
                TxtStatus.Text = $"已记录倍速 {speed:0.###}x（尚未附加）";
            }
            TxtSearch.ToolTip = "搜索窗口标题 / 进程名";
        RebuildPresets();
            UpdateSpeedState();
        }
        catch (Exception ex)
        {
            ShowError(ex.Message);
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
            BtnSettings.IsEnabled = true;
            TxtStatus.Text = "快捷键已保存";
            UpdateHotkeyText();
            return;
        }

        _recording = true;
        _recorded.Clear();
        TxtStatus.Text = "录制中：请按下手柄组合键，再点一次“修改快捷键”保存；右键可恢复默认";
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
            $"当前快捷键：{GamepadService.FormatHotkey(_settings.HotkeyButtons)}\n\n" +
            "提示：主界面点“修改快捷键”可手柄录制。\n关于：FastMode 0.2.0 · WPF 原生 · 加速 DLL 独立实现")
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
                TxtSearch.ToolTip = "搜索窗口标题 / 进程名";
        RebuildPresets();
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
            TxtStatus.Text = "已恢复默认 LB + RB";
            e.Handled = true;
            return;
        }
        base.OnMouseRightButtonUp(e);
    }

    private void OnGamepadUpdated(GamepadStatus status, bool edge)
    {
        Dispatcher.Invoke(() =>
        {
            TxtGamepad.Text = status.Connected ? $"已连接：{status.Name}" : "未连接";
            if (_recording)
            {
                if (status.ButtonsPressed.Count > 0)
                {
                    _recorded = status.ButtonsPressed.Distinct().OrderBy(x => x).ToList();
                    UpdateHotkeyText();
                }
                return;
            }

            if (!edge) return;
            if (!_speed.State.Attached)
            {
                TxtStatus.Text = "手柄触发：请先附加进程";
                return;
            }
            try
            {
                var next = !_settings.Enabled;
                var st = _speed.SetSpeed(_settings.CurrentSpeed, next);
                _settings.Enabled = next;
                SettingsService.Save(_settings);
                ChkEnable.IsChecked = next;
                TxtStatus.Text = "手柄切换 · " + st.Message;
                UpdateSpeedState();
            }
            catch (Exception ex)
            {
                ShowError(ex.Message);
            }
        });
    }

    private void UpdateHotkeyText()
    {
        if (_recording)
        {
            TxtHotkey.Text = _recorded.Count == 0
                ? "录制中：等待按键…"
                : "录制中：" + GamepadService.FormatHotkey(_recorded) + "（再点修改保存）";
        }
        else
        {
            TxtHotkey.Text = "切换键：" + GamepadService.FormatHotkey(_settings.HotkeyButtons);
        }
    }

    private void UpdateSpeedState()
    {
        var sp = _settings.CurrentSpeed;
        var en = _settings.Enabled && _speed.State.Attached;
        TxtSpeedState.Text = $"当前：{sp:0.###}x · {(en ? "已启用" : "未启用")}";
    }

    private void ShowError(string msg)
    {
        BannerError.Visibility = Visibility.Visible;
        TxtError.Text = msg;
        TxtStatus.Text = "错误";
    }

    private void ClearError()
    {
        BannerError.Visibility = Visibility.Collapsed;
        TxtError.Text = "";
    }
}

