using System;
using System.Runtime.InteropServices;
class SetPos
{
    [DllImport("user32.dll")] static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] static extern bool GetCursorPos(out POINT p);
    struct POINT { public int x; public int y; }
    static int Main(string[] args)
    {
        int x = args.Length > 0 ? int.Parse(args[0]) : 1280;
        int y = args.Length > 1 ? int.Parse(args[1]) : 720;
        SetCursorPos(x, y);
        POINT p; GetCursorPos(out p);
        Console.WriteLine("cursor now: " + p.x + "," + p.y);
        return 0;
    }
}
