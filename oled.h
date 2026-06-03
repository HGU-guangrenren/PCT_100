#ifndef __OLED_H
#define __OLED_H

#include "Arduino.h"

#define OLED_SDA_PIN    4
#define OLED_SCL_PIN    5

// 阈值 (运行期可变, MQTT 远程设置, NVS 持久化)
extern float g_temp_threshold;
extern int   g_light_threshold;

#define LUX_DIVISOR     30000.0f

void oled_set_temp_threshold(float t);
void oled_set_light_threshold(int l);

void lcd_init(void);
void lcd_update(bool is_auto_mode, bool main_on,
                int light_val, float temp_val,
                bool led_on, bool fan_on);

void oled_init(void);
void oled_update(void);
void oled_read_sensors(void);

float oled_get_voltage(void);
float oled_get_temp(void);
int   oled_get_lux(void);

#endif