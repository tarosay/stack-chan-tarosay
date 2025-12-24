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
    public sealed class TachikomaFaceRenderer : IDisposable
    {
        const int W = 320, H = 240;
        const int R = 38, D = 76;

        static readonly Point L0 = new Point(68, 63);
        static readonly Point R0 = new Point(176, 63);

        readonly Bitmap bg;
        readonly Bitmap eyeBase;
        readonly Bitmap iris;
        readonly Bitmap overlay;

        public TachikomaFaceRenderer(string bgPath, string eyeBasePath, string irisPath, string overlayPath)
        {
            bg = (Bitmap)Image.FromFile(bgPath);
            eyeBase = (Bitmap)Image.FromFile(eyeBasePath);
            iris = (Bitmap)Image.FromFile(irisPath);
            overlay = (Bitmap)Image.FromFile(overlayPath);
        }

        public Bitmap RenderFrame(int lookX, int lookY)
        {
            // 可動範囲を固定（Core2向け確定値）
            lookX = Clamp(lookX, -7, +7);
            lookY = Clamp(lookY, -5, +5);

            var frame = new Bitmap(W, H, PixelFormat.Format32bppArgb);

            using (var g = Graphics.FromImage(frame))
            {
                g.CompositingMode = CompositingMode.SourceOver;
                g.CompositingQuality = CompositingQuality.HighSpeed;
                g.SmoothingMode = SmoothingMode.None;                // ドット絵寄りならNone
                g.InterpolationMode = InterpolationMode.NearestNeighbor;
                g.PixelOffsetMode = PixelOffsetMode.Half;

                // 1) 背景
                g.DrawImage(bg, 0, 0, W, H);

                // 2) 目（ベース）左右
                g.DrawImage(eyeBase, L0.X, L0.Y, D, D);
                g.DrawImage(eyeBase, R0.X, R0.Y, D, D);

                // 3) アイリス（左右）…“iris.pngの中心が目の中心”になる前提で、平行移動だけ
                g.DrawImage(iris, L0.X + lookX, L0.Y + lookY, D, D);
                g.DrawImage(iris, R0.X + lookX, R0.Y + lookY, D, D);

                // 4) HUD
                g.DrawImage(overlay, 0, 0, W, H);
            }

            return frame;
        }

        public void Dispose()
        {
            bg?.Dispose();
            eyeBase?.Dispose();
            iris?.Dispose();
            overlay?.Dispose();
        }

        static int Clamp(int v, int min, int max) => (v < min) ? min : (v > max) ? max : v;
    }
}