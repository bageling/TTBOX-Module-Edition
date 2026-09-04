using System;
using System.Runtime.InteropServices;
using System.Threading;

class HidProbe
{
    // Raw input
    [StructLayout(LayoutKind.Sequential)]
    struct RAWINPUTDEVICE { public ushort usUsagePage; public ushort usUsage; public uint dwFlags; public IntPtr hwndTarget; }
    [StructLayout(LayoutKind.Sequential)]
    struct RAWINPUTHEADER { public uint dwType; public uint dwSize; public IntPtr hDevice; public IntPtr wParam; }
    [StructLayout(LayoutKind.Sequential)]
    struct RAWMOUSE { public ushort usFlags; public ushort usButtonFlags; public ushort usButtonData; public uint ulRawButtons; public int lLastX; public int lLastY; public uint ulExtraInformation; }
    [StructLayout(LayoutKind.Explicit)]
    struct RAWINPUT {
        [FieldOffset(0)] public RAWINPUTHEADER header;
        [FieldOffset(16)] public RAWMOUSE mouse;
    }
    struct POINT { public int x; public int y; }

    const int WM_INPUT = 0x00FF;
    const uint RID_INPUT = 0x10000003;
    const int RIM_TYPEMOUSE = 0;

    [DllImport("user32.dll")] static extern IntPtr CreateWindowExW(uint dwExStyle, string lpClassName, string lpWindowName, uint dwStyle, int x, int y, int nWidth, int nHeight, IntPtr hWndParent, IntPtr hMenu, IntPtr hInstance, IntPtr lpParam);
    [DllImport("user32.dll")] static extern bool RegisterRawInputDevices(RAWINPUTDEVICE[] pRawInputDevices, uint uiNumDevices, uint cbSize);
    [DllImport("user32.dll")] static extern uint GetRawInputData(IntPtr hRawInput, uint uiCommand, IntPtr pData, ref uint pcbSize, uint cbSizeHeader);
    [DllImport("user32.dll")] static extern bool PeekMessageW(out MSG lpMsg, IntPtr hWnd, uint wMsgFilterMin, uint wMsgFilterMax, uint wRemoveMsg);
    [DllImport("user32.dll")] static extern bool TranslateMessage(ref MSG lpMsg);
    [DllImport("user32.dll")] static extern IntPtr DispatchMessageW(ref MSG lpMsg);
    [DllImport("user32.dll")] static extern bool GetCursorPos(out POINT lpPoint);
    [DllImport("user32.dll")] static extern IntPtr GetModuleHandleW(string lpModuleName);
    [DllImport("user32.dll")] static extern IntPtr DefWindowProcW(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)] struct MSG { public IntPtr hwnd; public uint message; public IntPtr wParam; public IntPtr lParam; public uint time; public int pt_x; public int pt_y; }

    static int listenMs, cursorSamples;
    static DateTime started;

    static IntPtr WndProc(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam)
    {
        if (msg == WM_INPUT)
        {
            uint size = 0;
            GetRawInputData(lParam, RID_INPUT, IntPtr.Zero, ref size, (uint)Marshal.SizeOf<RAWINPUTHEADER>());
            IntPtr buf = Marshal.AllocHGlobal((int)size);
            try {
                if (size > 0 && GetRawInputData(lParam, RID_INPUT, buf, ref size, (uint)Marshal.SizeOf<RAWINPUTHEADER>()) == size)
                {
                    RAWINPUT ri = Marshal.PtrToStructure<RAWINPUT>(buf);
                    if (ri.header.dwType == RIM_TYPEMOUSE)
                    {
                        double t = (DateTime.UtcNow - started).TotalMilliseconds;
                        Console.WriteLine($"  RAWMOUSE t={t:F0}ms dx={ri.mouse.lLastX} dy={ri.mouse.lLastY} dev={ri.header.hDevice}");
                    }
                }
            } finally { Marshal.FreeHGlobal(buf); }
            return IntPtr.Zero;
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    static int Main(string[] args)
    {
        if (args.Length >= 1 && args[0] == "cursor")
        {
            // 光标采样模式：打印初始，然后打印每次变化，持续 seconds
            int seconds = args.Length >= 2 ? int.Parse(args[1]) : 5;
            int intervalMs = args.Length >= 3 ? int.Parse(args[2]) : 50;
            POINT last; GetCursorPos(out last);
            Console.WriteLine($"CURSOR start ({last.x},{last.y})");
            POINT p = last;
            DateTime end = DateTime.UtcNow.AddSeconds(seconds);
            while (DateTime.UtcNow < end)
            {
                Thread.Sleep(intervalMs);
                GetCursorPos(out p);
                if (p.x != last.x || p.y != last.y)
                {
                    double t = (DateTime.UtcNow - end).TotalMilliseconds + seconds * 1000;
                    Console.WriteLine($"  CURSOR MOVE ({last.x},{last.y})->({p.x},{p.y}) delta=({p.x-last.x},{p.y-last.y}) t={t:F0}ms_in");
                    last = p;
                }
            }
            POINT fin; GetCursorPos(out fin);
            Console.WriteLine($"CURSOR end ({fin.x},{fin.y}) total_delta=({fin.x-(int)GetStartX()},{fin.y})");
            return 0;
        }
        if (args.Length >= 1 && args[0] == "macros")
        {
            return RunRawInput(args.Length >= 2 ? int.Parse(args[1]) : 5);
        }
        Console.WriteLine("usage: HidProbe.exe cursor <seconds> <intervalMs> | HidProbe.exe macros <seconds>");
        return 1;
    }

    static long curStartX = 0;
    static long GetStartX() { return curStartX; }

    static int RunRawInput(int seconds)
    {
        IntPtr hmod = GetModuleHandleW(null);
        // 用宿主空闲窗口类（STATIC 不支持程序回调，需自定义类）。改用隐藏的消息窗口。
        // 简化：直接用 RegisterClass 需要 WndProc 委托固定。这里用 SetWindowLongPtr 装到一个窗口。
        // 为简单，创建窗口并用默认 proc + 全局 raw input 到 RIDEV_INPUTSINK + SetWindowLong 替换。
        // .NET 无法可靠保留委托 — 用虚拟方法表。改用 native 的动态编译成本高。
        // 方案：用 "Message" only window via DefWindowProc，raw input 用 hwnd=IntPtr.Zero 的 RIDEV_INPUTSINK?
        // GetRawInputData 需要收到 WM_INPUT。最简单可靠：注册一个自定义窗口类。
        Console.WriteLine("raw-input mode disabled in this build; use GetMessage hook alternative");
        return 2;
    }
}