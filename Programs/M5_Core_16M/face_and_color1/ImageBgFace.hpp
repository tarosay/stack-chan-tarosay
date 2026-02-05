#ifndef IMAGEBGFACE_HPP
#define IMAGEBGFACE_HPP
// ImageBgFace.hpp — extracted from face_and_color.ino (2026-02-05)
// A/B個体差対策：各パーツのdraw()冒頭でBase updateを必ず通す

#include <Avatar.h>
#include <M5Unified.h>
#include <Eyes.hpp>
#include <Mouths.hpp>
#include <faces/FaceTemplates.hpp>
#include <algorithm>
#include <stdint.h>

#include "motoko.hpp"

using namespace m5avatar;

// 背景画像（RGB565配列）は .ino 側で motoko.hpp を include して提供する
extern const uint16_t motokos[];

// ========= Custom Eye =========
class SlitEye : public m5avatar::BaseEye {
public:
  using BaseEye::BaseEye;  // (width, height, is_left)

  void draw(M5Canvas* c, m5avatar::BoundingRect rect, m5avatar::DrawContext* ctx) override {
    // ★ベース更新を明示的に通す（open_ratio_/gaze_を確定させる）
    BaseEye::update(c, rect, ctx);

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
    // ★ベース更新を明示的に通す（口開きを確定させる）
    BaseMouth::update(c, rect, ctx);

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

      // 右目・左目
      new SlitEye(56, 40, false), new BoundingRect(160, 96),  // 右目
      new SlitEye(56, 40, true), new BoundingRect(160, 224),  // 左目

      // 右眉・左眉
      new BowEyebrow(64, 20, false), new BoundingRect(120, 96),
      new BowEyebrow(64, 20, true), new BoundingRect(120, 224)) {}
};

#endif
