#ifndef __CONSOLE_H
#define __CONSOLE_H

#include "Arduino.h"

// ============================================================================
// 串口命令行级分发器 (解决多 module 共享 Serial 读输入的竞争)
//
// 用法 (在 loop() 中):
//   console_pump();                  // 从 Serial 读一行, 暂存
//   mqtt_mgr_console();              // mqtt 取一行, 不匹配则退回
//   rgb_led_console();               // rgb 取一行, 不匹配则退回
//   (其他 module 同理)
//
// 不匹配的 module 调 console_give_back() 把 line 放回, 下个 module 再取
// ============================================================================

void    console_pump(void);
String  console_take(void);
void    console_give_back(const String& line);

#endif
