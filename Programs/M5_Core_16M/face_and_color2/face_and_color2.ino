#include <Avatar.h>
#include <M5GFX.h>
#include <M5Unified.h>
#include <Eyes.hpp>    // 追加
#include <Mouths.hpp>  // 追加
#include <faces/FaceTemplates.hpp>

static constexpr uint16_t palette[16] = {
  0x0000,  // 0  BLACK
  0xFFFF,  // 1  WHITE
  0xF800,  // 2  RED
  0x07E0,  // 3  GREEN
  0x001F,  // 4  BLUE（純青）
  0xFFE0,  // 5  YELLOW
  0x07FF,  // 6  CYAN
  0xF81F,  // 7  MAGENTA
  0x8410,  // 8  GRAY
  0x03EF,  // 9  DARK CYAN
  0x7BEF,  // 10 DARK GRAY
  0xAFE5,  // 11 LIGHT GREEN
  0xC618,  // 12 LIGHT GRAY
  0x0010,  // 13 NAVY（暗い青）
  0xF81F,  // 14 PINK
  0xFFFF   // 15 WHITE（予備）
};


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

#include "ImageBgFace.hpp"

void setup() {
  M5.begin();
  M5.Lcd.setBrightness(30);
  M5.Lcd.clear();

  faces[0] = new ImageBgFace();
  faces[1] = new DoggyFace();
  faces[2] = new OmegaFace();
  faces[3] = new GirlyFace();
  faces[4] = new PinkDemonFace();
  //faces[5] = new MyCustomFace();
  //faces[5] = new MyFullCustomFace();  // ←ここで登録
  //faces[5] = new ImageOnlyFace();  // ←ここで登録
  faces[5] = avatar.getFace();  // native face

  // auto* gfx = &M5.Display;  // M5Unifiedの描画先
  //gfx->setPalette(palette);

  color_palettes[0] = new ColorPalette();
  color_palettes[1] = new ColorPalette();
  color_palettes[2] = new ColorPalette();
  color_palettes[3] = new ColorPalette();
  color_palettes[4] = new ColorPalette();

  color_palettes[1]->set(COLOR_PRIMARY, palette[13]);
  color_palettes[2]->set(COLOR_PRIMARY, palette[13]);
  color_palettes[3]->set(COLOR_PRIMARY, palette[13]);
  color_palettes[4]->set(COLOR_PRIMARY, palette[13]);

  color_palettes[1]->set(COLOR_BACKGROUND, palette[2]);
  color_palettes[2]->set(COLOR_BACKGROUND, palette[3]);
  color_palettes[3]->set(COLOR_BACKGROUND, palette[4]);
  color_palettes[4]->set(COLOR_BACKGROUND, palette[5]);

  color_palettes[1]->set(COLOR_SECONDARY, palette[6]);
  color_palettes[2]->set(COLOR_SECONDARY, palette[6]);
  color_palettes[3]->set(COLOR_SECONDARY, palette[6]);
  color_palettes[4]->set(COLOR_SECONDARY, palette[6]);



  //avatar.init(8);  // start drawing w/ 8bit color mode
  avatar.setIndexedPalette(palette, 16);
  avatar.init(4);  // start drawing w/ 8bit color mode
  avatar.setColorPalette(*color_palettes[0]);
  avatar.setFace(faces[0]);
}

void loop() {
  M5.update();
  if (M5.BtnA.wasPressed()) {
    avatar.setFace(faces[face_idx]);
    face_idx = (face_idx + 1) % num_faces;  // loop index
  }
  if (M5.BtnB.wasPressed()) {
    avatar.suspend();  // ★追加
    avatar.setColorPalette(*color_palettes[palette_idx]);
    avatar.resume();  // ★追加

    //face_idx = face_idx - 1 < 0 ? num_faces - 1 : face_idx - 1;
    //avatar.setFace(faces[face_idx]);
    //face_idx = (face_idx + 1) % num_faces;  // loop index
    palette_idx = (palette_idx + 1) % num_palettes;
  }
  if (M5.BtnC.wasPressed()) {
    avatar.setExpression(expressions[idx]);
    idx = (idx + 1) % num_expressions;
  }
}
