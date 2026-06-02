#ifndef __OLED_H
#define __OLED_H

#include "Arduino.h"

#define OLED_SDA_PIN    4
#define OLED_SCL_PIN    5

#define TEMP_THRESHOLD  30.0f
#define LUX_THRESHOLD   300
#define LUX_DIVISOR     30000.0f

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