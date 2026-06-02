#ifndef __RGB_LED_H
#define __RGB_LED_H

#include "Arduino.h"

#define RGB_LED_PIN         0
#define RGB_LED_COUNT       1
#define RGB_LED_BRIGHTNESS  60

typedef enum {
    RGB_MODE_BOOT = 0,
    RGB_MODE_BOOT_SUCCESS,
    RGB_MODE_WIFI_OK,
    RGB_MODE_WIFI_DISC,
    RGB_MODE_TEST,
} rgb_mode_t;

void rgb_led_init(void);
void rgb_led_set_mode(rgb_mode_t m);
void rgb_led_test_color(uint8_t r, uint8_t g, uint8_t b);
void rgb_led_exit_test_mode(void);
void rgb_led_trigger_boot_success(void);
void rgb_led_update(void);
void rgb_led_console(void);

#endif
