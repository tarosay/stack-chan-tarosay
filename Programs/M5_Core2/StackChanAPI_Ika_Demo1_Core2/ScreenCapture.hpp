#ifndef SCREEN_CAPTURE_HPP
#define SCREEN_CAPTURE_HPP

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define FILENAME "/capture.dat"

class ScreenCapture {
public:
  bool save();
  bool load();
  bool load(const char* datpath);
  void loadbin(const uint16_t* name);
private:
};

// グローバルインスタンスの宣言
extern ScreenCapture ScCaptur;

#endif  // SCREEN_CAPTURE_HPP
