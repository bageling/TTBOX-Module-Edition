using System;
using System.Runtime.InteropServices;
using System.IO;
class CursorPoll
{
    [DllImport("user32.dll")] static extern bool GetCursorPos(out POINT p);
    struct POINT { public int x; public int y; }
    static int Main(string[] args)
    {
        string outPath = args.Length > 0 ? args[0] : "cursorlog.txt";
        int ms = args.Length > 1 ? int.Parse(args[1]) : 3000;
        int interval = args.Length > 2 ? int.Parse(args[2]) : 20;
        using (StreamWriter sw = new StreamWriter(outPath, false))
        {
            long start = DateTime.UtcNow.Ticks / TimeSpan.TicksPerMillisecond;
            POINT p; GetCursorPos(out p);
            sw.WriteLine("T=0 X=" + p.x + " Y=" + p.y);
            while (DateTime.UtcNow.Ticks / TimeSpan.TicksPerMillisecond - start < ms)
            {
                System.Threading.Thread.Sleep(interval);
                GetCursorPos(out p);
                long t = DateTime.UtcNow.Ticks / TimeSpan.TicksPerMillisecond - start;
                sw.WriteLine("T=" + t + " X=" + p.x + " Y=" + p.y);
            }
        }
        return 0;
    }
}
