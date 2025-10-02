#include <Avatar.h>
#include <M5Unified.h>
#include <Eyes.hpp>    // 追加
#include <Mouths.hpp>  // 追加
#include <faces/FaceTemplates.hpp>

#include "motoko.hpp"

using namespace m5avatar;

Avatar avatar;

Face* faces[6];
const int num_faces = sizeof(faces) / sizeof(Face*);
int face_idx = 5;  // face index

const Expression expressions[] = { Expression::Angry, Expression::Sleepy,
                                   Expression::Happy, Expression::Sad,
                                   Expression::Doubt, Expression::Neutral };
const int num_expressions = sizeof(expressions) / sizeof(Expression);
int idx = 0;

ColorPalette* color_palettes[5];
const int num_palettes = sizeof(color_palettes) / sizeof(ColorPalette*);
int palette_idx = 0;

bool isShowingQR = false;


// ========= Custom Eye =========
class SlitEye : public m5avatar::BaseEye {
public:
  using BaseEye::BaseEye;  // (width, height, is_left)

  void draw(M5Canvas* c, m5avatar::BoundingRect rect, m5avatar::DrawContext* ctx) override {
    // ── 色は毎回スナップショット（左右で同一に）
    auto* pal = ctx->getColorPalette();
    const uint16_t skin = pal->get(COLOR_BACKGROUND);  // まぶた（肌）
    const uint16_t line = TFT_BLACK;                   // 枠＆瞳は黒で固定（左右差を確実に消す）
    const uint16_t white = TFT_WHITE;                  // 白目は白で固定

    // ── 中心＆サイズ（中心は rect、サイズは BaseEye の width_/height_）
    const int cx = rect.getLeft();
    const int cy = rect.getTop();
    const int rx = width_ / 2;
    const int ry = height_ / 2;

    // ── 開き（縦半径）を楕円で表現（四角マスクは使わない）
    const float open = std::max(0.0f, open_ratio_);
    const int oy = std::max(8, (int)(ry * open));  // 縦半径の下限=8px（つぶれ防止）

    // 1) まず“目の領域”を肌色で満たす（まぶたベース）
    c->fillEllipse(cx, cy, rx, ry, skin);

    // 2) 開いている部分を白目で描く（縦半径 oy の楕円：丸いスリットになる）
    c->fillEllipse(cx, cy, rx - 1, oy, white);

    // 3) 枠線は“開口楕円”に沿って引く（左右で同色）
    c->drawEllipse(cx, cy, rx, oy, line);

    // 4) 瞳（黒）— 開口に合わせて中で動かす
    const int px = cx + (int)(gaze_.getHorizontal() * rx * 0.4f);
    const int py = cy + (int)(-gaze_.getVertical() * oy * 0.6f);

    // 瞳半径：開きに追従しつつ、下限/上限を持たせる
    const float open01 = std::max(0.0f, std::min(1.0f, open_ratio_));
    const int baseR = 5;                    // ← 最小半径（好みで 6〜8 に）
    const float gain = 0.50f;               // ← 開きに対する増分（0.4〜0.7で調整）
    const int maxR = std::min(rx, oy) - 2;  // はみ出し防止
    int pr = (int)(baseR + gain * std::min(rx, oy) * open01);
    pr = std::max(baseR, std::min(pr, maxR));  // クランプ

    c->fillCircle(px, py, pr, line);
    c->fillCircle(px - pr / 3, py - pr / 3, std::max(1, pr / 4), TFT_WHITE);
  }
};

// ========= Custom Mouth (fix) =========
class WaveMouth : public m5avatar::BaseMouth {
public:
  // BaseMouth(minW, maxW, minH, maxH)
  WaveMouth(uint16_t w, uint16_t h)
    : BaseMouth(w, w, 2, h) {}

  void draw(M5Canvas* c, m5avatar::BoundingRect rect, m5avatar::DrawContext* ctx) override {
    // ── 色を決める ──
    const uint16_t line = TFT_WHITE;                       // 枠は白
    const uint16_t inner = M5.Lcd.color24to16(0x7A1C1C);   // 口の中は暗赤
    const uint16_t lip = M5.Lcd.color24to16(0xE88AAE);     // 唇ピンク
    const uint16_t tongue = M5.Lcd.color24to16(0xFF6B6B);  // 舌

    // ── 中心座標 ──
    const int cx = rect.getLeft();
    const int cy = rect.getTop();

    // ── サイズ ──
    const int wMax = (int)std::max(min_width_, max_width_);
    const int hMax = (int)std::max(min_height_, max_height_);

    // ★ 縦を強調：倍率を 0.25 → 0.5 に変更
    const float open = std::max(0.0f, ctx->getMouthOpenRatio());
    const int ow = wMax;
    const int oh = std::max(16, (int)(hMax * (0.5f + open)));  // ←縦長

    // ── 唇（外側） ──
    c->fillRoundRect(cx - ow / 2, cy - oh / 2, ow, oh, 8, lip);
    c->drawRoundRect(cx - ow / 2, cy - oh / 2, ow, oh, 8, line);

    // ── 口の中 ──
    c->fillRoundRect(cx - (ow - 6) / 2, cy - (oh - 6) / 2, ow - 6, oh - 6, 6, inner);

    // ── 舌（下半分に表示） ──
    const int tongueH = std::max(0, (int)(oh * 0.35f * open));
    if (tongueH > 0) {
      c->fillRoundRect(cx - (ow - 12) / 2, cy + (oh / 2) - tongueH, ow - 12, tongueH, 6, tongue);
    }
  }
};

// ★ 背景画像→口 を 1 スロットで描くラッパー
class MouthOverImage : public m5avatar::Drawable {
  m5avatar::BaseMouth* mouth_;
public:
  explicit MouthOverImage(m5avatar::BaseMouth* mouth)
    : mouth_(mouth) {}

  void draw(M5Canvas* c, m5avatar::BoundingRect rect, m5avatar::DrawContext* ctx) override {
    // 1) 先に“顔画像”を全面に描く（キャンバス中央寄せ）
    const int screenW = c->width();
    const int screenH = c->height();
    const int x = (screenW - 320) / 2;
    const int y = (screenH - 240) / 2;
    c->pushImage(x, y, 320, 240, motokos);  // ← motoko.hpp の RGB565 配列

    // 2) 次に本来の口を描く（update→draw 順で呼ぶ）
    mouth_->update(c, rect, ctx);
    mouth_->draw(c, rect, ctx);
  }

  ~MouthOverImage() override {
    delete mouth_;
  }
};

class ImageBgFace : public Face {
public:
  ImageBgFace()
    : Face(
      // 口スロットに「画像＋口」の複合Drawable
      new MouthOverImage(new WaveMouth(/*幅*/ 120, /*高さ*/ 48)),
      new BoundingRect(200, 160),  // 口の中心 (y,x) 例

      // 右目・左目（あなたの SlitEye など既存の目をそのまま）
      new SlitEye(56, 40, false), new BoundingRect(160, 96),  // 右目
      new SlitEye(56, 40, true), new BoundingRect(160, 224),  // 左目

      // 右眉・左眉
      new BowEyebrow(64, 20, false), new BoundingRect(120, 96),
      new BowEyebrow(64, 20, true), new BoundingRect(120, 224)) {}
};

// ========= Face 組み立て =========
class MyFullCustomFace : public Face {
public:
  MyFullCustomFace()
    : Face(
      new WaveMouth(64, 28), new BoundingRect(222, 160),
      new SlitEye(56, 40, false), new BoundingRect(163, 64),  // 右目
      new SlitEye(56, 40, true), new BoundingRect(163, 256),  // 左目
      new BowEyebrow(64, 20, false), new BoundingRect(128, 64),
      new BowEyebrow(64, 20, true), new BoundingRect(128, 256)) {}
};


// 何も描かないダミー
class EmptyDrawable : public m5avatar::Drawable {
public:
  void draw(M5Canvas*, m5avatar::BoundingRect, m5avatar::DrawContext*) override {}
};

// 画面に画像を貼る Drawable（rectは無視して全体に描く）
class FullImageDrawable : public m5avatar::Drawable {
public:
  void draw(M5Canvas* c, m5avatar::BoundingRect, m5avatar::DrawContext*) override {
    // 画像サイズに合わせて座標・サイズを調整
    constexpr int W = 320;  // ヘッダに無ければ数値直書き例: 240
    constexpr int H = 240;  // 同上: 240
    // 画面左上に貼る。中央にしたければ ( (M5.Lcd.width()-W)/2, (M5.Lcd.height()-H)/2 )
    c->pushImage(0, 0, W, H, motokos);
  }
};

class ImageOnlyFace : public m5avatar::Face {
public:
  ImageOnlyFace()
    : Face(
      // mouth枠にフル画像を割り当て（どのスロットでもOK）
      new FullImageDrawable(), new m5avatar::BoundingRect(M5.Lcd.height() / 2, M5.Lcd.width() / 2),
      // 以降は何も描かないダミーで埋める
      new EmptyDrawable(), new m5avatar::BoundingRect(0, 0),
      new EmptyDrawable(), new m5avatar::BoundingRect(0, 0),
      new EmptyDrawable(), new m5avatar::BoundingRect(0, 0),
      new EmptyDrawable(), new m5avatar::BoundingRect(0, 0)) {}
};







// an example of customizing
// class MyCustomFace : public Face {
// public:
//   MyCustomFace()
//     : Face(new UShapeMouth(44, 44, 0, 16), new BoundingRect(222, 160),
//            // right eye, second eye arg is center position of eye
//            new EllipseEye(32, 32, false), new BoundingRect(163, 64),
//            //  left eye
//            new EllipseEye(32, 32, true), new BoundingRect(163, 256),
//            // right eyebrow
//            // BowEyebrow's origin is the center of bow (arc)
//            new BowEyebrow(64, 20, false),
//            new BoundingRect(163, 64),  // (y,x)
//                                        //  left eyebrow
//            new BowEyebrow(64, 20, true), new BoundingRect(163, 256)) {}
// };

void setup() {
  M5.begin();
  M5.Lcd.setBrightness(30);
  M5.Lcd.clear();

  faces[0] = avatar.getFace();  // native face
  faces[1] = new DoggyFace();
  faces[2] = new OmegaFace();
  faces[3] = new GirlyFace();
  faces[4] = new PinkDemonFace();
  //faces[5] = new MyCustomFace();
  //faces[5] = new MyFullCustomFace();  // ←ここで登録
  //faces[5] = new ImageOnlyFace();  // ←ここで登録
  faces[5] = new ImageBgFace();

  color_palettes[0] = new ColorPalette();
  color_palettes[1] = new ColorPalette();
  color_palettes[2] = new ColorPalette();
  color_palettes[3] = new ColorPalette();
  color_palettes[4] = new ColorPalette();
  color_palettes[1]->set(COLOR_PRIMARY,
                         M5.Lcd.color24to16(0x383838));  // eye
  color_palettes[1]->set(COLOR_BACKGROUND,
                         M5.Lcd.color24to16(0xfac2a8));  // skin
  color_palettes[1]->set(COLOR_SECONDARY,
                         TFT_PINK);  // cheek
  color_palettes[2]->set(COLOR_PRIMARY, TFT_YELLOW);
  color_palettes[2]->set(COLOR_BACKGROUND, TFT_DARKCYAN);
  color_palettes[3]->set(COLOR_PRIMARY, TFT_DARKGREY);
  color_palettes[3]->set(COLOR_BACKGROUND, TFT_WHITE);
  color_palettes[4]->set(COLOR_PRIMARY, TFT_RED);
  color_palettes[4]->set(COLOR_BACKGROUND, TFT_PINK);

  avatar.init(8);  // start drawing w/ 8bit color mode
  avatar.setColorPalette(*color_palettes[0]);
}

void loop() {
  M5.update();
  // M5Stack Core's button layout:
  // -----------
  // |         |
  // |         |
  // -----------
  // [A] [B] [C]
  if (M5.BtnA.wasPressed()) {
    avatar.setFace(faces[face_idx]);
    face_idx = (face_idx + 1) % num_faces;  // loop index
  }
  if (M5.BtnB.wasPressed()) {
    avatar.setColorPalette(*color_palettes[palette_idx]);
    palette_idx = (palette_idx + 1) % num_palettes;
  }
  if (M5.BtnC.wasPressed()) {
    avatar.setExpression(expressions[idx]);
    idx = (idx + 1) % num_expressions;
  }
}
