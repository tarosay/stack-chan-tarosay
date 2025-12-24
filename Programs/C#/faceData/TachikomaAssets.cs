using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace faceData
{
    static class TachikomaAssets
    {
        public static Bitmap MakeBg320x240()
        {
            var bmp = new Bitmap(320, 240, PixelFormat.Format32bppArgb);
            using (var g = Graphics.FromImage(bmp))
            {
                g.SmoothingMode = SmoothingMode.AntiAlias;

                // ベゼル（青装甲）
                g.Clear(Color.FromArgb(255, 30, 90, 160));

                // ガラス面
                var glass = new Rectangle(19, 19, 320 - 38, 240 - 38);
                using (var br = new SolidBrush(Color.FromArgb(255, 10, 20, 40)))
                    g.FillRectangle(br, glass);

                // 反射（斜め帯）
                using (var pen = new Pen(Color.FromArgb(35, 220, 240, 255), 6))
                    g.DrawLine(pen, 40, 35, 265, 85);


                // ボルトっぽい点（四隅寄り）
                using (var br = new SolidBrush(Color.FromArgb(120, 10, 20, 40)))
                {
                    int[] xs = { 12, 320 - 12 };
                    int[] ys = { 12, 240 - 12 };
                    foreach (var x in xs) foreach (var y in ys) g.FillEllipse(br, x - 3, y - 3, 6, 6);
                }
            }
            return bmp;
        }

        public static Bitmap MakeEyeBase76()
        {
            int D = 76; int R = 38;
            var bmp = new Bitmap(D, D, PixelFormat.Format32bppArgb);
            using (var g = Graphics.FromImage(bmp))
            {
                g.SmoothingMode = SmoothingMode.AntiAlias;
                g.Clear(Color.Transparent);

                // 外リング
                using (var pen = new Pen(Color.FromArgb(255, 170, 200, 230), 4))
                    g.DrawEllipse(pen, 2, 2, D - 4, D - 4);

                // 内リング
                using (var pen = new Pen(Color.FromArgb(140, 60, 110, 170), 2))
                    g.DrawEllipse(pen, 8, 8, D - 16, D - 16);

                // 目盛り（薄く）
                using (var pen = new Pen(Color.FromArgb(60, 220, 240, 255), 1))
                {
                    for (int i = 0; i < 12; i++)
                    {
                        double a = i * (Math.PI * 2 / 12);
                        int x1 = (int)(R + Math.Cos(a) * 30);
                        int y1 = (int)(R + Math.Sin(a) * 30);
                        int x2 = (int)(R + Math.Cos(a) * 34);
                        int y2 = (int)(R + Math.Sin(a) * 34);
                        g.DrawLine(pen, x1, y1, x2, y2);
                    }
                }
            }
            return bmp;
        }

        public static Bitmap MakeIris76()
        {
            int D = 76; int R = 38;
            var bmp = new Bitmap(D, D, PixelFormat.Format32bppArgb);
            using (var g = Graphics.FromImage(bmp))
            {
                g.SmoothingMode = SmoothingMode.AntiAlias;
                g.Clear(Color.Transparent);

                // アイリス（明るめ青）
                using (var br = new SolidBrush(Color.FromArgb(220, 80, 170, 240)))
                    g.FillEllipse(br, R - 21, R - 21, 42, 42);

                // 中央絞り（暗紺：真っ黒にしない）
                using (var br = new SolidBrush(Color.FromArgb(240, 20, 40, 70)))
                    g.FillEllipse(br, R - 11, R - 11, 22, 22);

                // ハイライト点
                using (var br = new SolidBrush(Color.FromArgb(200, 240, 250, 255)))
                    g.FillEllipse(br, R - 10, R - 14, 6, 6);
            }
            return bmp;
        }

        public static Bitmap MakeOverlay320x240()
        {
            var bmp = new Bitmap(320, 240, PixelFormat.Format32bppArgb);
            using (var g = Graphics.FromImage(bmp))
            {
                g.SmoothingMode = SmoothingMode.None;
                g.Clear(Color.Transparent);

                // スキャンライン（薄く）
                using (var pen = new Pen(Color.FromArgb(12, 255, 255, 255), 1))
                    for (int y = 0; y < 240; y += 3) g.DrawLine(pen, 0, y, 319, y);

                // 角のL字HUD
                using (var pen = new Pen(Color.FromArgb(60, 220, 240, 255), 2))
                {
                    void L(int x, int y, int dx, int dy)
                    {
                        g.DrawLine(pen, x, y, x + dx, y);
                        g.DrawLine(pen, x, y, x, y + dy);
                    }
                    L(12, 12, 18, 0 + 18);
                    L(320 - 12, 12, -18, 18);
                    L(12, 240 - 12, 18, -18);
                    L(320 - 12, 240 - 12, -18, -18);
                }

                // 小さい赤点（アクセント）
                using (var br = new SolidBrush(Color.FromArgb(120, 255, 60, 60)))
                    g.FillEllipse(br, 320 - 20, 20, 6, 6);
            }
            return bmp;
        }
    }
}