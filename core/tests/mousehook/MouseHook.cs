using System;
using System.Runtime.InteropServices;
using System.IO;
using System.Text;

class LowLevelMouseHook
{
    const int WH_MOUSE_LL = 14;
    const uint WM_MOUSEMOVE = 0x0200;
    [StructLayout(LayoutKind.Sequential)]
    struct POINT { public int x; public int y; }
    [StructLayout(LayoutKind.Sequential)]
    struct MSLLHOOKSTRUCT { public POINT pt; public uint mouseData; public uint flags; public uint time; public IntPtr dwExtraInfo; }
    delegate IntPtr LowLevelMouseProc(int nCode, IntPtr wParam, ref MSLLHOOKSTRUCT lParam);
    [DllImport("user32.dll")] static extern IntPtr SetWindowsHookExW(int idHook, LowLevelMouseProc lpfn, IntPtr hMod, uint dwThreadId);
    [DllImport("user32.dll")] static extern bool UnhookWindowsHookEx(IntPtr hhk);
    [DllImport("user32.dll")] static extern IntPtr CallNextHookEx(IntPtr hhk, int nCode, IntPtr wParam, ref MSLLHOOKSTRUCT lParam);
    [DllImport("kernel32.dll")] static extern IntPtr GetModuleHandleW(string lpModuleName);
    [DllImport("user32.dll")] static extern bool GetCursorPos(out POINT lpPoint);

    static IntPtr hook;
    static StreamWriter sw;
    static DateTime start;

    static IntPtr Proc(int nCode, IntPtr wParam, ref MSLLHOOKSTRUCT lParam)
    {
        if (nCode >= 0)
        {
            uint msg = (uint)wParam;
            POINT cur; GetCursorPos(out cur);
            long now = (DateTime.UtcNow - start).Ticks / TimeSpan.TicksPerMillisecond;
            if (msg == WM_MOUSEMOVE)
            {
                sw.WriteLine(string.Format("MOVE pt=({0},{1}) cursor=({2},{3}) time={4}ms msgtime={5}",
                    lParam.pt.x, lParam.pt.y, cur.x, cur.y, now, lParam.time));
            }
            else if (msg >= 0x0201 && msg <= 0x0206)
            {
                sw.WriteLine(string.Format("BTN msg={0} pt=({1},{2}) cursor=({3},{4}) time={5}ms",
                    msg, lParam.pt.x, lParam.pt.y, cur.x, cur.y, now));
            }
            sw.Flush();
        }
        return CallNextHookEx(hook, nCode, wParam, ref lParam);
    }

    static int Main(string[] args)
    {
        string outPath = args.Length > 0 ? args[0] : "mouselog.txt";
        int seconds = args.Length > 1 ? int.Parse(args[1]) : 10;
        sw = new StreamWriter(new FileStream(outPath, FileMode.Create), Encoding.UTF8);
        start = DateTime.UtcNow;
        POINT lastCursor; GetCursorPos(out lastCursor);
        sw.WriteLine(string.Format("START cursor=({0},{1}) listening {2}s", lastCursor.x, lastCursor.y, seconds));
        sw.Flush();

        LowLevelMouseProc proc = Proc;
        hook = SetWindowsHookExW(WH_MOUSE_LL, proc, GetModuleHandleW(null), 0);
        if (hook == IntPtr.Zero)
        {
            sw.WriteLine("SetWindowsHookExW FAILED err=" + Marshal.GetLastWin32Error());
            sw.Flush(); sw.Close();
            return 1;
        }
        sw.WriteLine("Hook installed OK");
        sw.Flush();

        DateTime end = DateTime.UtcNow.AddSeconds(seconds);
        MSG msg; IntPtr r;
        while (DateTime.UtcNow < end)
        {
            r = GetMessage(out msg, IntPtr.Zero, 0, 0);
            if (r == (IntPtr)0 || r == (IntPtr)(-1)) break;
            TranslateMessage(ref msg);
            DispatchMessage(ref msg);
        }
        sw.WriteLine("END");
        sw.Flush();
        UnhookWindowsHookEx(hook);
        sw.Close();
        return 0;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct MSG { public IntPtr hwnd; public uint message; public IntPtr wParam; public IntPtr lParam; public uint time; public int pt_x; public int pt_y; }
    [DllImport("user32.dll")] static extern IntPtr GetMessage(out MSG lpMsg, IntPtr hWnd, uint wMsgFilterMin, uint wMsgFilterMax);
    [DllImport("user32.dll")] static extern bool TranslateMessage(ref MSG lpMsg);
    [DllImport("user32.dll")] static extern IntPtr DispatchMessage(ref MSG lpMsg);
}