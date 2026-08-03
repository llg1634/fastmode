using System.Runtime.InteropServices;

namespace FastMode.Services;

public sealed class GamepadStatus
{
    public bool Connected { get; init; }
    public string? Name { get; init; }
    public List<int> ButtonsPressed { get; init; } = new();
}

public sealed class GamepadService : IDisposable
{
    private readonly object _gate = new();
    private bool _prevChord;
    private CancellationTokenSource? _cts;
    private GamepadStatus _status = new();

    public event Action<GamepadStatus, bool>? Updated; // status, chordEdge

    public GamepadStatus Status
    {
        get { lock (_gate) return _status; }
    }

    public static readonly IReadOnlyDictionary<int, string> ButtonNames = new Dictionary<int, string>
    {
        [0] = "A", [1] = "B", [2] = "X", [3] = "Y",
        [4] = "LB", [5] = "RB", [6] = "LT", [7] = "RT",
        [8] = "Back", [9] = "Start", [10] = "L3", [11] = "R3",
        [12] = "Up", [13] = "Down", [14] = "Left", [15] = "Right", [16] = "Guide"
    };

    public static string FormatHotkey(IEnumerable<int> buttons)
    {
        var list = buttons.ToList();
        if (list.Count == 0) return LocalizationService.Get("L10n.NotSet");
        return string.Join(" + ", list.Select(FormatButton));
    }

    private static string FormatButton(int button) => button switch
    {
        12 => LocalizationService.Get("L10n.DirectionUp"),
        13 => LocalizationService.Get("L10n.DirectionDown"),
        14 => LocalizationService.Get("L10n.DirectionLeft"),
        15 => LocalizationService.Get("L10n.DirectionRight"),
        _ => ButtonNames.TryGetValue(button, out var name) ? name : "#" + button
    };

    public void Start(Func<IReadOnlyList<int>> getHotkey)
    {
        Stop();
        _cts = new CancellationTokenSource();
        var token = _cts.Token;
        Task.Run(async () =>
        {
            while (!token.IsCancellationRequested)
            {
                var hotkey = getHotkey()?.ToList() ?? new List<int> { 4, 5 };
                var (status, pressed) = Read();
                var chord = hotkey.Count > 0 && hotkey.All(pressed.Contains);
                bool edge;
                lock (_gate)
                {
                    edge = chord && !_prevChord;
                    _prevChord = chord;
                    _status = new GamepadStatus
                    {
                        Connected = status.Connected,
                        Name = status.Name,
                        ButtonsPressed = pressed
                    };
                }
                Updated?.Invoke(Status, edge);
                try { await Task.Delay(33, token); } catch { break; }
            }
        }, token);
    }

    public void Stop()
    {
        _cts?.Cancel();
        _cts = null;
    }

    private static (GamepadStatus status, List<int> pressed) Read()
    {
        for (uint i = 0; i < 4; i++)
        {
            var state = new XINPUT_STATE();
            if (XInputGetState(i, ref state) == 0)
            {
                var pressed = MapButtons(state.Gamepad.wButtons, state.Gamepad.bLeftTrigger, state.Gamepad.bRightTrigger);
                return (new GamepadStatus
                {
                    Connected = true,
                    Name = LocalizationService.Format("L10n.ControllerNameTemplate", i),
                    ButtonsPressed = pressed
                }, pressed);
            }
        }
        return (new GamepadStatus { Connected = false }, new List<int>());
    }

    private static List<int> MapButtons(ushort w, byte lt, byte rt)
    {
        var v = new List<int>();
        void add(ushort mask, int idx) { if ((w & mask) != 0) v.Add(idx); }
        add(0x1000, 0); // A
        add(0x2000, 1); // B
        add(0x4000, 2); // X
        add(0x8000, 3); // Y
        add(0x0100, 4); // LB
        add(0x0200, 5); // RB
        add(0x0020, 8); // Back
        add(0x0010, 9); // Start
        add(0x0040, 10);
        add(0x0080, 11);
        add(0x0001, 12);
        add(0x0002, 13);
        add(0x0004, 14);
        add(0x0008, 15);
        if (lt > 30) v.Add(6);
        if (rt > 30) v.Add(7);
        return v;
    }

    public void Dispose() => Stop();

    #region xinput
    [StructLayout(LayoutKind.Sequential)]
    struct XINPUT_GAMEPAD
    {
        public ushort wButtons;
        public byte bLeftTrigger;
        public byte bRightTrigger;
        public short sThumbLX;
        public short sThumbLY;
        public short sThumbRX;
        public short sThumbRY;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct XINPUT_STATE
    {
        public uint dwPacketNumber;
        public XINPUT_GAMEPAD Gamepad;
    }

    [DllImport("xinput1_4.dll", EntryPoint = "XInputGetState")]
    static extern uint XInputGetState_1_4(uint dwUserIndex, ref XINPUT_STATE pState);

    [DllImport("xinput9_1_0.dll", EntryPoint = "XInputGetState")]
    static extern uint XInputGetState_9_1_0(uint dwUserIndex, ref XINPUT_STATE pState);

    static uint XInputGetState(uint idx, ref XINPUT_STATE state)
    {
        try { return XInputGetState_1_4(idx, ref state); }
        catch (DllNotFoundException)
        {
            return XInputGetState_9_1_0(idx, ref state);
        }
    }
    #endregion
}

