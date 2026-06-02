#include "oled.h"
#include <U8g2lib.h>
#include <Wire.h>
#include "adc.h"
#include "ds18b20.h"

extern bool get_power_on(void);
extern int  get_mode(void);
extern bool get_auto_mode(void);
extern bool led_state(void);
extern bool fan_state(void);

static U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0,
  /* reset=*/ U8X8_PIN_NONE,
  /* clock=*/ OLED_SCL_PIN,
  /* data=*/ OLED_SDA_PIN
);

static unsigned long last_update = 0;
static float last_voltage = 0.0f;
static float last_temp    = 0.0f;
static int   last_lux     = 0;

void oled_read_sensors(void)
{
    int raw = adc_read_raw();
    float voltage = raw * (3.3f / 4095.0f);
    int inv = 4095 - raw;
    if (inv < 0) inv = 0;
    long lux_long = ((long)inv * (long)inv) / (long)LUX_DIVISOR;
    int lux = (int)lux_long;

    ds18b20_update();
    float temp = ds18b20_read_temp();

    last_voltage = voltage;
    last_temp    = temp;
    last_lux     = lux;
}

static const char* mode_label(int mode, bool autoMode)
{
    (void)mode;
    if (autoMode) return "自动";
    return "手动";
}

void lcd_init(void) {
    u8g2.begin();
    u8g2.enableUTF8Print();
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2.drawUTF8(28, 35, "系统启动中...");
    u8g2.sendBuffer();
    delay(1000);
}

void lcd_update(bool is_auto_mode, bool main_on,
                int light_val, float temp_val,
                bool led_on, bool fan_on) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);

    if (main_on) {
        char buf[24];

        u8g2.setCursor(0, 12);
        u8g2.print("模式:");
        u8g2.print(mode_label(0, is_auto_mode));
        u8g2.print("  总闸:开");

        snprintf(buf, sizeof(buf), "光照: %d/%d", light_val, LUX_THRESHOLD);
        u8g2.setCursor(0, 26);
        u8g2.print(buf);

        snprintf(buf, sizeof(buf), "温度: %.1f/%.1f", temp_val, TEMP_THRESHOLD);
        u8g2.setCursor(0, 40);
        u8g2.print(buf);

        u8g2.setCursor(0, 54);
        u8g2.print("灯:");
        u8g2.setCursor(36, 54);
        u8g2.print(led_on ? "开" : "关");
        u8g2.setCursor(72, 54);
        u8g2.print("风扇:");
        u8g2.setCursor(108, 54);
        u8g2.print(fan_on ? "开" : "关");
    } else {
        u8g2.setCursor(0, 14);
        u8g2.print("总闸:关");
        u8g2.setCursor(0, 40);
        u8g2.print("系统关闭");
    }

    u8g2.sendBuffer();
}

void oled_init(void) {
    lcd_init();
}

void oled_update(void)
{
    unsigned long now = millis();
    if (now - last_update < 500) return;
    last_update = now;

    lcd_update(get_auto_mode(), get_power_on(),
               last_lux, last_temp,
               led_state(), fan_state());
}

float oled_get_voltage(void) { return last_voltage; }
float oled_get_temp(void)    { return last_temp; }
int   oled_get_lux(void)     { return last_lux; }