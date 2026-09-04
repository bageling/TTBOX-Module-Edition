using System;
using System.Drawing;
using System.Windows.Forms;

class FullscreenImage
{
    [STAThread]
    static void Main(string[] args)
    {
        string path = args.Length > 0 ? args[0] : null;
        Application.EnableVisualStyles();
        Form f = new Form();
        f.FormBorderStyle = FormBorderStyle.None;
        f.WindowState = FormWindowState.Maximized;
        f.TopMost = true;
        f.KeyPreview = true;
        f.KeyDown += (s, e) => { if (e.KeyCode == Keys.Escape) Application.Exit(); };
        PictureBox pb = new PictureBox();
        pb.Dock = DockStyle.Fill;
        pb.SizeMode = PictureBoxSizeMode.Zoom;
        if (path != null && System.IO.File.Exists(path))
            pb.Image = Image.FromFile(path);
        f.Controls.Add(pb);
        Application.Run(f);
    }
}
