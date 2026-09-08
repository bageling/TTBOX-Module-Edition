using System;
using System.Runtime.InteropServices;
class GetPos
{
    [DllImport("user32.dll")] static extern bool GetCursorPos(out POINT p);
    struct POINT { public int x; public int y; }
    static int Main()
    {
        POINT p; GetCursorPos(out p);
        Console.WriteLine(p.x + "," + p.y);
        return 0;
    }
}
