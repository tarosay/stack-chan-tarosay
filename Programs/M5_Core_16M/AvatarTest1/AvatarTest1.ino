#include <Avatar.h>
#include <M5Unified.h>

m5avatar::Avatar avatar;

const m5avatar::Expression expressions[] = { m5avatar::Expression::Angry, m5avatar::Expression::Sleepy,
                                             m5avatar::Expression::Happy, m5avatar::Expression::Sad,
                                             m5avatar::Expression::Doubt, m5avatar::Expression::Neutral };
const int num_expressions = sizeof(expressions) / sizeof(m5avatar::Expression);
int idx = 0;

void setup() {
  Serial.begin(115200);
  delay(1500);
  M5.begin();
  M5.Lcd.setBrightness(30);
  M5.Lcd.clear();

  avatar.init(8);  // start drawing w/ 8bit color mode
  avatar.setFace(avatar.getFace());
  avatar.setMouthOpenRatio(0.0f);
}
bool lip = false;
void loop() {
  M5.update();
  if (M5.BtnA.wasPressed()) {
    if (lip) {
      lip = false;
      avatar.setMouthOpenRatio(0.0f);
    } else {
      lip = true;
    }
  }
  if (M5.BtnB.wasPressed()) {
  }
  if (M5.BtnC.wasPressed()) {
    avatar.setExpression(expressions[idx]);
    idx = (idx + 1) % num_expressions;
  }

  if (lip) {
    float phase = (millis() % 400) / 400.0f;                     // 200ms周期
    float tri = (phase < 0.5f) ? (phase * 2) : (2 - phase * 2);  // 0→1→0
    avatar.setMouthOpenRatio(tri);
  }
}
