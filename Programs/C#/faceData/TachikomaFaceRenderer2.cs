using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace faceData
{
    public sealed class TachikomaFaceRenderer2 : IDisposable
    {
        const int W = 320, H = 240, R = 38, D = 76;
        static readonly Point L0 = new Point(68, 63);
        static readonly Point R0 = new Point(176, 63);

        readonly Bitmap bg, eyeBase, iris, overlay;

        public TachikomaFaceRenderer2(Bitmap bg, Bitmap eyeBase, Bitmap iris, Bitmap overlay)
        { this.bg = bg; this.eyeBase = eyeBase; this.iris = iris; this.overlay = overlay; }

        public Bitmap RenderFrame(int lookX, int lookY)
        {
            lookX = Math.Max(-7, Math.Min(7, lookX));
            lookY = Math.Max(-5, Math.Min(5, lookY));

            var frame = new Bitmap(W, H, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
            using (var g = Graphics.FromImage(frame))
            {
                g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.NearestNeighbor;
                g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.None;

                g.DrawImage(bg, 0, 0);
                g.DrawImage(eyeBase, L0.X, L0.Y, D, D);
                g.DrawImage(eyeBase, R0.X, R0.Y, D, D);
                g.DrawImage(iris, L0.X + lookX, L0.Y + lookY, D, D);
                g.DrawImage(iris, R0.X + lookX, R0.Y + lookY, D, D);
                g.DrawImage(overlay, 0, 0);
            }
            return frame;
        }

        public void Dispose() { bg?.Dispose(); eyeBase?.Dispose(); iris?.Dispose(); overlay?.Dispose(); }
    }
}