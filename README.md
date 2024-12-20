# stack-chan-tarosay
オリジナルのスタックチャンです

stackchan-arduinoのソースを修正しています。

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
 