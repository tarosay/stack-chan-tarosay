using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace faceData
{
    public partial class Form1 : Form
    {
        // Core2想定サイズ
        const int W = 320;
        const int H = 240;

        // 見た目パラメータ（ここをいじって色味チェック）
        Color faceBlue = Color.FromArgb(255, 30, 120, 220);     // 背景：明るい青
        Color rimBlue = Color.FromArgb(255, 120, 190, 255);    // 縁：薄い青
        Color screwMetal = Color.FromArgb(255, 200, 210, 220);  // ねじ色

        int eyeR = 110;     // 白目半径
        int rimW = 10;      // 縁太さ
        int dotR = 8;      // 黒点半径
        int triR = 18;      // 三角形（中心→頂点）半径
        int limitR = 200;   // 黒点セット（三角形中心）の移動範囲半径

        int screwR = 18;    // ねじ半径
        int screwRimW = 2;          // ねじ外周の縁線幅
        int screwSlotW = 6;         // マイナス溝の線幅（太くするならここ）
        float screwSlotLen = 0.65f; // 溝の長さ（半径に対する割合）
        int screwRimAlpha = 80;     // 縁の影の濃さ(0-255)

        bool animate = true;

        Timer timer;
        float t;

        public Form1()
        {
            InitializeComponent();
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            // 既存画像があれば破棄
            this.pictureBox1.Image?.Dispose();
            this.pictureBox1.Image = null;


        }

        private void timer1_Tick(object sender, EventArgs e)
        {
            if (animate)
            {
                Random rnd = new Random();
                // 0.0 以上 1.0 未満の乱数を生成
                double randomValue = rnd.NextDouble();

                t += (float)(0.033f * (8 * randomValue));
            }

            // 1フレーム生成
            using (var frame = RenderFrame(t))
            {
                // 差し替え（古いImageを必ずDispose）
                var old = pictureBox1.Image;
                pictureBox1.Image = (Bitmap)frame.Clone();
                old?.Dispose();
            }
        }

        private void Form1_FormClosed(object sender, FormClosedEventArgs e)
        {
            timer1.Enabled = false;
            timer1?.Dispose();

            pictureBox1.Image?.Dispose();
            pictureBox1.Image = null;
        }
        Bitmap RenderFrame(float time)
        {
            var bmp = new Bitmap(W, H, PixelFormat.Format32bppArgb);
            using (var g = Graphics.FromImage(bmp))
            {
                g.SmoothingMode = SmoothingMode.AntiAlias;

                // 1) 背景：明るい青
                g.Clear(faceBlue);

                // 2) 四隅マイナスねじ

                float screwMargin = 6f;                 // 画面端からの余白
                float sx = screwR + screwMargin;
                float sy = screwR + screwMargin;

                DrawScrew(g, sx, sy);
                DrawScrew(g, W - sx, sy);
                DrawScrew(g, sx, H - sy);
                DrawScrew(g, W - sx, H - sy);

                // 3) 中央の白目＋薄青縁
                var eyeCenter = new PointF(W * 0.5f, H * 0.52f);

                using (var brWhite = new SolidBrush(Color.White))
                    g.FillEllipse(brWhite, eyeCenter.X - eyeR, eyeCenter.Y - eyeR, eyeR * 2, eyeR * 2);

                using (var penRim = new Pen(rimBlue, rimW))
                    g.DrawEllipse(penRim, eyeCenter.X - eyeR, eyeCenter.Y - eyeR, eyeR * 2, eyeR * 2);

                // 4) 黒点3つ（正三角形頂点）= 1セットの眼
                // 5) そのセットが白目内を動く
                float margin = 2f;

                // 白目の内側半径（縁の内側）
                float eyeInnerR = eyeR - rimW * 0.5f;

                // 三角形中心が動ける最大半径（黒丸が欠けない条件）
                float maxCenterR = eyeInnerR - triR - dotR - margin;
                if (maxCenterR < 0) maxCenterR = 0;

                float movable = Math.Min(limitR, maxCenterR);       // ユーザー指定を優先しつつ安全側に制限


                float cx = animate ? (float)(Math.Sin(time * 1.25) * movable) : 0f;
                float cy = animate ? (float)(Math.Sin(time * 0.93 + 1.4) * movable) : 0f;

                // ★重要：半径movableを超えたら円周上に押し戻す（√2で超えるのを防ぐ）
                float rr = cx * cx + cy * cy;
                float mm = movable * movable;
                if (rr > mm && rr > 0f)
                {
                    float s = (float)Math.Sqrt(mm / rr);
                    cx *= s;
                    cy *= s;
                }

                float triRotate = animate ? (time * 0.8f) : 0f; // 三角形回転（不要なら0）

                var triCenter = new PointF(eyeCenter.X + cx, eyeCenter.Y + cy);

                DrawDot(g, triCenter, triR, triRotate + 0f);
                DrawDot(g, triCenter, triR, triRotate + 2.0943951f); // +120°
                DrawDot(g, triCenter, triR, triRotate + 4.1887902f); // +240°
            }
            return bmp;
        }

        void DrawScrew(Graphics g, float x, float y)
        {
            float r = screwR;

            // ねじ頭
            using (var br = new SolidBrush(screwMetal))
                g.FillEllipse(br, x - r, y - r, r * 2, r * 2);

            // 外周の薄い影（立体感）
            using (var pen = new Pen(Color.FromArgb(screwRimAlpha, 0, 0, 0), screwRimW))
                g.DrawEllipse(pen, x - r, y - r, r * 2, r * 2);

            // マイナス溝（幅＝screwSlotW）
            float halfLen = r * screwSlotLen;
            using (var pen = new Pen(Color.FromArgb(170, 40, 40, 40), screwSlotW))
            {
                pen.StartCap = LineCap.Round; // 端を丸く（それっぽい）
                pen.EndCap = LineCap.Round;
                g.DrawLine(pen, x - halfLen, y, x + halfLen, y);
            }
        }

        void DrawDot(Graphics g, PointF center, float triR, float angleRad)
        {
            float px = center.X + (float)Math.Cos(angleRad) * triR;
            float py = center.Y + (float)Math.Sin(angleRad) * triR;

            using (var br = new SolidBrush(Color.Black))
                g.FillEllipse(br, px - dotR, py - dotR, dotR * 2, dotR * 2);
        }

    }
}
