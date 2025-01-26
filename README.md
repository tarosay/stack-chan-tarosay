# stack-chan-tarosay
オリジナルのスタックチャンです

## コンパイルに必要なライブラリ

- **M5Stack by M5Stack**: [GitHub Repository](https://github.com/m5stack/m5stack)
- **M5Stack-SD-Updater by tobozo**: [GitHub Repository](https://github.com/tobozo/M5Stack-SD-Updater/)
- **YAMLDuino by tobozo**: [GitHub Repository](https://github.com/tobozo/YAMLDuino)
- **ServoEasing by Ammin Joachimsmeyer**: [GitHub Repository](https://github.com/ArminJo/ServoEasing)
- **SCServo by FT&WS**: [GitHub Repository](https://github.com/workloads/scservo)
- **Dynamixel2Arduino by ROBOTIS**: [GitHub Repository](https://github.com/ROBOTIS-GIT/dynamixel2arduino)
- **M5Stack_Avatar by Shinya Ishikawa**: [PlatformIO Registry](https://registry.platformio.org/libraries/meganetaaan/M5Stack-Avatar)

## SCSCL::WritePos()のエラー修正

使っているライブラリが実は違います。本来は以下を使います。

[https://github.com/mongonta0716/SCServo](https://github.com/mongonta0716/SCServo)

一番簡単な変更方法は、インストールしたSCServoのsrcのフォルダの中身を、上のリポジトリからダウンロードしたソースに置き換えてしまう方法です。

または、以下のように、stackchan-arduinoのソースを書き換えます。

コンパイルすると、以下のエラーがでます。
Stackchan_servo.cpp:58:128: error: no matching function for call to 'SCSCL::WritePos(int, long int, int)'
     _sc.WritePos(AXIS_X + 1, convertSCS0009Pos(_init_param.servo[AXIS_X].start_degree + _init_param.servo[AXIS_X].offset), 1000);
     
     In file included from d:\Users\minao\Documents\Arduino\libraries\SCServo\src/SCServo.h:9,
                 from d:\Users\minao\Documents\Arduino\libraries\stackchan-arduino\src\Stackchan_servo.h:12,
                 from d:\Users\minao\Documents\Arduino\libraries\stackchan-arduino\src\Stackchan_servo.cpp:2:
d:\Users\minao\Documents\Arduino\libraries\SCServo\src/SCSCL.h:55:17: note: candidate: 'virtual int SCSCL::WritePos(u8, u16, u16, u16)'
     virtual int WritePos(u8 ID, u16 Position, u16 Time, u16 Speed); // Normal write of single servo position command
     

エラーを修正するために、
Documents\Arduino\libraries\stackchan-arduino\src\Stackchan_servo.cpp
にある
     _sc.WritePos(AXIS_X + 1, convertSCS0009Pos(_init_param.servo[AXIS_X].start_degree + _init_param.servo[AXIS_X].offset), 1000);
を
     _sc.WritePos(AXIS_X + 1, convertSCS0009Pos(_init_param.servo[AXIS_X].start_degree + _init_param.servo[AXIS_X].offset), 1000, 0);
 のように、
 全ての _sc.WritePos( の第4引数 ,0を追加します。
 
 stackchan-arduinoのライブラリがアップデートすると、同じエラーになるので、修正しましょう。
 
 class SCSCL は、シリアルサーボの制御クラスなので、PWM系のSG90を使う分には、この修正は影響ないです。
 
 
 https://github.com/stack-chan/stackchan-arduino/blob/main/README.md
 