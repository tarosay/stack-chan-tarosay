#ifndef SCREEN_CAPTURE_HPP
#define SCREEN_CAPTURE_HPP

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define FILENAME "/capture.dat"

class ScreenCapture {
public:
    bool save();
    bool load();

private:

};

// グローバルインスタンスの宣言
extern ScreenCapture ScCaptur;

#endif // SCREEN_CAPTURE_HPP
