#include "rgb_led.h"
#include "console.h"
#include <Adafruit_NeoPixel.h>

static Adafruit_NeoPixel strip(RGB_LED_COUNT, RGB_LED_PIN,
                                NEO_GRB + NEO_KHZ800);
static rgb_mode_t    cur_mode           = RGB_MODE_BOOT;
static bool          test_mode          = false;
static unsigned long boot_start         = 0;
static unsigned long boot_success_start = 0;
static unsigned long anim_ms            = 0;

static const uint32_t BOOT_PALETTE[] = {
    0xFF0000, 0xFF7F00, 0xFFFF00, 0x00FF00,
    0x00FFFF, 0x0000FF, 0x8B00FF, 0xFFFFFF
};
#define BOOT_PALETTE_SIZE  8
#define BOOT_STEP_MS       100
#define BOOT_ROUNDS        3
#define BOOT_TOTAL_MS      (BOOT_PALETTE_SIZE * BOOT_STEP_MS * BOOT_ROUNDS)
#define BOOT_SUCCESS_MS    1500

void rgb_led_init(void)
{
    strip.begin();
    strip.setBrightness(RGB_LED_BRIGHTNESS);
    strip.clear();
    strip.show();

    cur_mode  = RGB_MODE_BOOT;
    test_mode = false;
    boot_start = millis();
    anim_ms    = millis();
}

void rgb_led_set_mode(rgb_mode_t m)
{
    if (test_mode) return;
    if (cur_mode == m) return;
    cur_mode = m;
    anim_ms  = millis();
    strip.clear();
    strip.show();
}

rgb_mode_t rgb_led_get_mode(void)
{
    return cur_mode;
}

void rgb_led_test_color(uint8_t r, uint8_t g, uint8_t b)
{
    test_mode = true;
    cur_mode  = RGB_MODE_TEST;
    strip.setPixelColor(0, strip.Color(r, g, b));
    strip.show();
}

void rgb_led_exit_test_mode(void)
{
    test_mode = false;
    cur_mode  = RGB_MODE_OFF;
    strip.clear();
    strip.show();
}

void rgb_led_trigger_boot_success(void)
{
    if (test_mode) return;
    cur_mode = RGB_MODE_BOOT_SUCCESS;
    boot_success_start = millis();
    anim_ms = millis();
    strip.clear();
    strip.show();
}

void rgb_led_update(void)
{
    if (test_mode) return;

    unsigned long now = millis();

    switch (cur_mode) {
        // === 开机/庆祝 (保留原行为, 仅改结束后跳转目标) ===
        case RGB_MODE_BOOT: {
            unsigned long elapsed = now - boot_start;
            if (elapsed >= BOOT_TOTAL_MS) {
                rgb_led_set_mode(RGB_MODE_OFF);
            } else {
                uint8_t idx = (elapsed / BOOT_STEP_MS) % BOOT_PALETTE_SIZE;
                strip.setPixelColor(0, BOOT_PALETTE[idx]);
                strip.show();
            }
            break;
        }
        case RGB_MODE_BOOT_SUCCESS: {
            unsigned long elapsed = now - boot_success_start;
            if (elapsed >= BOOT_SUCCESS_MS) {
                rgb_led_set_mode(RGB_MODE_OFF);
            } else {
                uint32_t phase = ((uint32_t)(elapsed % 500UL) * 65535UL) / 500UL;
                strip.setPixelColor(0, strip.ColorHSV((uint16_t)phase, 255, 200));
                strip.show();
            }
            break;
        }

        // === #1 严重警报 (LED+Fan) - 红红绿绿蓝蓝, 1200ms ===
        case RGB_MODE_ALARM_SEVERE: {
            unsigned long e = (now - anim_ms) % 1200UL;
            if      (e < 100)  strip.setPixelColor(0, strip.Color(255, 0, 0));
            else if (e < 150)  strip.clear();
            else if (e < 250)  strip.setPixelColor(0, strip.Color(255, 0, 0));
            else if (e < 300)  strip.clear();
            else if (e < 400)  strip.setPixelColor(0, strip.Color(0, 255, 0));
            else if (e < 450)  strip.clear();
            else if (e < 550)  strip.setPixelColor(0, strip.Color(0, 255, 0));
            else if (e < 600)  strip.clear();
            else if (e < 700)  strip.setPixelColor(0, strip.Color(0, 0, 255));
            else if (e < 750)  strip.clear();
            else if (e < 850)  strip.setPixelColor(0, strip.Color(0, 0, 255));
            else               strip.clear();
            strip.show();
            break;
        }

        // === #2 光照越界 (仅LED) - 红300→绿300→蓝300, 1500ms ===
        case RGB_MODE_ALARM_LIGHT: {
            unsigned long e = (now - anim_ms) % 1500UL;
            if      (e < 300)  strip.setPixelColor(0, strip.Color(255, 0, 0));
            else if (e < 500)  strip.clear();
            else if (e < 800)  strip.setPixelColor(0, strip.Color(0, 255, 0));
            else if (e < 1000) strip.clear();
            else if (e < 1300) strip.setPixelColor(0, strip.Color(0, 0, 255));
            else               strip.clear();
            strip.show();
            break;
        }

        // === #3 温度越界 (仅风扇) - 红500→绿500→蓝500→灭700, 2200ms ===
        case RGB_MODE_ALARM_TEMP: {
            unsigned long e = (now - anim_ms) % 2200UL;
            if      (e < 500)  strip.setPixelColor(0, strip.Color(255, 0, 0));
            else if (e < 1000) strip.setPixelColor(0, strip.Color(0, 255, 0));
            else if (e < 1500) strip.setPixelColor(0, strip.Color(0, 0, 255));
            else               strip.clear();
            strip.show();
            break;
        }

        // === #5 WiFi 未连 - 红200→绿200→灭500, 900ms ===
        case RGB_MODE_WIFI_DISC: {
            unsigned long e = (now - anim_ms) % 900UL;
            if      (e < 200)  strip.setPixelColor(0, strip.Color(255, 0, 0));
            else if (e < 400)  strip.setPixelColor(0, strip.Color(0, 255, 0));
            else               strip.clear();
            strip.show();
            break;
        }

        // === #4 MQTT 未连 - HSV 渐变, 2000ms ===
        case RGB_MODE_MQTT_DISC: {
            unsigned long e = (now - anim_ms) % 2000UL;
            // 0-500: Red(H=0) → Green(H=21845), V=255
            if (e < 500) {
                float t = (float)e / 500.0f;
                uint16_t hue = (uint16_t)(t * 21845.0f);
                strip.setPixelColor(0, strip.ColorHSV(hue, 255, 255));
            // 500-1000: Green(H=21845) → Off, V:255→0
            } else if (e < 1000) {
                float t = (float)(e - 500) / 500.0f;
                uint8_t val = (uint8_t)(255.0f * (1.0f - t));
                strip.setPixelColor(0, strip.ColorHSV(21845, 255, val));
            // 1000-1500: Off → Blue(H=43690), V:0→255
            } else if (e < 1500) {
                float t = (float)(e - 1000) / 500.0f;
                uint8_t val = (uint8_t)(255.0f * t);
                strip.setPixelColor(0, strip.ColorHSV(43690, 255, val));
            // 1500-2000: Blue(H=43690) → Off, V:255→0
            } else {
                float t = (float)(e - 1500) / 500.0f;
                uint8_t val = (uint8_t)(255.0f * (1.0f - t));
                strip.setPixelColor(0, strip.ColorHSV(43690, 255, val));
            }
            strip.show();
            break;
        }

        // === #6 全部正常 - HSV 渐变循环, 4200ms ===
        case RGB_MODE_IDLE: {
            unsigned long e = (now - anim_ms) % 4200UL;
            // 0-1000: Off → Red, H=0, V:0→255
            if (e < 1000) {
                float t = (float)e / 1000.0f;
                uint8_t val = (uint8_t)(255.0f * t);
                strip.setPixelColor(0, strip.ColorHSV(0, 255, val));
            // 1000-2000: Red→Green, H:0→21845, V=255
            } else if (e < 2000) {
                float t = (float)(e - 1000) / 1000.0f;
                uint16_t hue = (uint16_t)(t * 21845.0f);
                strip.setPixelColor(0, strip.ColorHSV(hue, 255, 255));
            // 2000-3000: Green→Blue, H:21845→43690, V=255
            } else if (e < 3000) {
                float t = (float)(e - 2000) / 1000.0f;
                uint16_t hue = 21845 + (uint16_t)(t * 21845.0f);
                strip.setPixelColor(0, strip.ColorHSV(hue, 255, 255));
            // 3000-4000: Blue→Off, H=43690, V:255→0
            } else if (e < 4000) {
                float t = (float)(e - 3000) / 1000.0f;
                uint8_t val = (uint8_t)(255.0f * (1.0f - t));
                strip.setPixelColor(0, strip.ColorHSV(43690, 255, val));
            // 4000-4200: Off hold 200ms
            } else {
                strip.clear();
            }
            strip.show();
            break;
        }

        // === OFF / TEST / default ===
        default:
            strip.clear();
            strip.show();
            break;
    }
}

void rgb_led_console(void)
{
    String line = console_take();
    if (line.length() == 0) return;

    if (line.equalsIgnoreCase("LED OFF") || line.equalsIgnoreCase("LED AUTO")) {
        rgb_led_exit_test_mode();
        Serial.println("[RGB] Exit test mode, back to AUTO");
        return;
    }

    if (line.startsWith("RGB ")) {
        String arg = line.substring(4);
        arg.trim();
        uint8_t r = 0, g = 0, b = 0;

        if (arg.startsWith("#") && arg.length() == 7) {
            long hex = strtoul(arg.substring(1).c_str(), NULL, 16);
            r = (uint8_t)((hex >> 16) & 0xFF);
            g = (uint8_t)((hex >>  8) & 0xFF);
            b = (uint8_t)( hex        & 0xFF);
        } else {
            int sp1 = arg.indexOf(' ');
            int sp2 = arg.indexOf(' ', sp1 + 1);
            if (sp1 < 0 || sp2 < 0) {
                Serial.println("[RGB] Format: 'RGB R G B' or 'RGB #RRGGBB'");
                return;
            }
            r = (uint8_t)arg.substring(0, sp1).toInt();
            g = (uint8_t)arg.substring(sp1 + 1, sp2).toInt();
            b = (uint8_t)arg.substring(sp2 + 1).toInt();
        }

        rgb_led_test_color(r, g, b);
        Serial.printf("[RGB] Test mode: R=%d G=%d B=%d  (#%02X%02X%02X)\n",
                      r, g, b, r, g, b);
    }
    else {
        // 非本模块命令, 退回给下个 console
        console_give_back(line);
    }
}
