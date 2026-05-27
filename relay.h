#ifndef __RELAY_H
#define __RELAY_H

#include "Arduino.h"

// 继电器控制引脚定义（根据电路图，IN端连接到ESP32C3）
#define RELAY_PIN     1   // 继电器控制引脚

// 继电器状态定义
#define RELAY_ON      HIGH   // 高电平触发（对应电路图的H端）
#define RELAY_OFF     LOW    // 低电平关闭

// 继电器控制宏
#define RELAY(x)      digitalWrite(RELAY_PIN, x)
#define RELAY_TOGGLE() digitalWrite(RELAY_PIN, !digitalRead(RELAY_PIN))

// 函数声明
void relay_init(void);      // 初始化继电器引脚
void relay_on(void);        // 打开继电器
void relay_off(void);       // 关闭继电器
void relay_toggle(void);    // 翻转继电器状态

#endif