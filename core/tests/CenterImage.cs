// CenterImage.cs — 把指定图片缩放到 size 居中（或偏移）显示，其余黑色背景
// 用法: CenterImage.exe <image> [size] [offset_x] [offset_y]
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;

class CenterImage
{
    [STAThread]
    static void Main(string[] args)
    {
        string path = args.Length > 0 ? args[0] : null;
        int size = args.Length > 1 ? int.Parse(args[1]) : 500;
        int offX = args.Length > 2 ? int.Parse(args[2]) : 0;
        int offY = args.Length > 3 ? int.Parse(args[3]) : 0;
        Application.EnableVisualStyles();
        Form f = new Form();
        f.FormBorderStyle = FormBorderStyle.None;
        f.WindowState = FormWindowState.Maximized;
        f.TopMost = true;
        f.KeyPreview = true;
        f.BackColor = Color.Black;
        f.KeyDown += (s, e) => { if (e.KeyCode == Keys.Escape) Application.Exit(); };
        f.Paint += (s, e) =>
        {
            e.Graphics.Clear(Color.Black);
            if (path == null || !System.IO.File.Exists(path)) return;
            using (var img = Image.FromFile(path))
            using (var bmp = new Bitmap(size, size))
            {
                using (var g = Graphics.FromImage(bmp))
                {
                    g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                    g.SmoothingMode = SmoothingMode.HighQuality;
                    g.PixelOffsetMode = PixelOffsetMode.HighQuality;
                    g.DrawImage(img, 0, 0, size, size);
                }
                // 居中 + 偏移绘制
                var rc = f.ClientRectangle;
                int x = rc.Left + (rc.Width - size) / 2 + offX;
                int y = rc.Top + (rc.Height - size) / 2 + offY;
                e.Graphics.DrawImage(bmp, x, y);
            }
        };
        f.Shown += (s, e) => f.Invalidate();
        var t = new Timer();
        t.Interval = 100;
        t.Tick += (s, e) => f.Invalidate();
        t.Start();
        Application.Run(f);
    }
}