using System.Windows;
using System.Windows.Input;

namespace FastMode;

public partial class OverlayWindow : Window
{
    public OverlayWindow()
    {
        InitializeComponent();
        // default place near top-right of primary work area
        var wa = SystemParameters.WorkArea;
        Left = wa.Right - 48;
        Top = wa.Top + 48;
    }

    public void SetEnabledState(bool on)
    {
        TxtState.Text = on ? "On" : "Off";
        Root.Background = on
            ? new System.Windows.Media.SolidColorBrush(System.Windows.Media.Color.FromRgb(0x3F, 0x9D, 0x6A))
            : new System.Windows.Media.SolidColorBrush(System.Windows.Media.Color.FromRgb(0x6A, 0x8A, 0x78));
    }

    private void Root_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (e.LeftButton == MouseButtonState.Pressed)
            DragMove();
    }
}
