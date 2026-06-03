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
    cur_mode  = RGB_MODE_WIFI_DISC;
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
    anim_ms = now;

    switch (cur_mode) {
        case RGB_MODE_BOOT: {
            unsigned long elapsed = now - boot_start;
            if (elapsed >= BOOT_TOTAL_MS) {
                rgb_led_set_mode(RGB_MODE_WIFI_DISC);
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
                rgb_led_set_mode(RGB_MODE_WIFI_DISC);
            } else {
                uint32_t phase = ((uint32_t)(elapsed % 500UL) * 65535UL) / 500UL;
                strip.setPixelColor(0, strip.ColorHSV((uint16_t)phase, 255, 200));
                strip.show();
            }
            break;
        }
        case RGB_MODE_WIFI_OK: {
            uint16_t phase = (uint16_t)(((uint32_t)(now % 3000UL) * 65535UL) / 3000UL);
            uint16_t tri = (phase < 32768) ? phase : (uint16_t)(65535 - phase);
            uint8_t  val = 13 + (uint8_t)((uint32_t)tri * 64UL / 32767UL);
            strip.setPixelColor(0, strip.ColorHSV(21845, 255, val));
            strip.show();
            break;
        }
        case RGB_MODE_WIFI_DISC: {
            uint16_t phase = (uint16_t)(((uint32_t)(now % 3000UL) * 65535UL) / 3000UL);
            uint16_t tri = (phase < 32768) ? phase : (uint16_t)(65535 - phase);
            uint8_t  val = 13 + (uint8_t)((uint32_t)tri * 64UL / 32767UL);
            strip.setPixelColor(0, strip.ColorHSV(0, 255, val));
            strip.show();
            break;
        }
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
