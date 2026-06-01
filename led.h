#ifndef __LED_H
#define __LED_H

#include "Arduino.h"

#define LED_PIN       6
#define FAN_PIN       7

#define LED_ON        HIGH
#define LED_OFF       LOW
#define FAN_ON        HIGH
#define FAN_OFF       LOW

void led_init(void);
void led_on(void);
void led_off(void);
void led_toggle(void);

void fan_init(void);
void fan_on(void);
void fan_off(void);
void fan_toggle(void);



#endif