#include "exti.h"
#include "led.h"
#include "adc.h"
#include "ds18b20.h"
#include "oled.h"
#include "version.h"
#include "wifi_mgr.h"

static bool powerOn = false;
static int mode = 0;
static bool autoMode = true;
static int lastManualMode = 4;

static unsigned long key2_down_time = 0;
static bool key2_holding = false;

#define LONG_PRESS_MIN   1000
#define LONG_PRESS_MAX   2000

void power_on(void)
{
    powerOn = true;
    mode = 4;
    led_off();
    fan_off();
    Serial.println("POWER ON");
}

void power_off(void)
{
    powerOn = false;
    mode = 0;
    autoMode = true;
    led_off();
    fan_off();
    Serial.println("POWER OFF - Reset to AUTO mode");
}

void set_mode(int m)
{
    if (!powerOn || autoMode) return;
    mode = m;
    switch (mode) {
        case 1: led_on(); fan_off(); Serial.println("Light ON"); break;
        case 2: led_off(); fan_on(); Serial.println("Fan ON"); break;
        case 3: led_on(); fan_on(); Serial.println("Both ON"); break;
        case 4: led_off(); fan_off(); Serial.println("Both OFF"); break;
    }
    lastManualMode = mode;
}

void toggle_auto(void)
{
    if (!powerOn) return;
    autoMode = !autoMode;
    if (autoMode) {
        mode = 4;
        led_off();
        fan_off();
        Serial.println("AUTO MODE - All OFF");
    } else {
        Serial.println("MANUAL MODE");
        mode = lastManualMode;
        switch (mode) {
            case 1: led_on(); fan_off(); break;
            case 2: led_off(); fan_on(); break;
            case 3: led_on(); fan_on(); break;
            case 4: led_off(); fan_off(); break;
        }
    }
}

bool get_power_on(void) { return powerOn; }
int  get_mode(void)     { return mode; }
bool get_auto_mode(void) { return autoMode; }

void setup()
{
    Serial.begin(115200);
    pinMode(6, OUTPUT);
    digitalWrite(6, HIGH);
    pinMode(8, OUTPUT);
    digitalWrite(8, LOW);

    for (int i = 0; i < 5; i++) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("--- SYSTEM STARTED ---");

    exti_init();
    led_init();
    fan_init();
    led_off();
    fan_off();
    digitalWrite(6, LOW);

    adc_init();
    ds18b20_init();
    oled_init();
    Serial.println("System initialized!");
    Serial.println("KEY1=Power  KEY2=short:Mode  hold:Auto  Default:AUTO");

    wifi_mgr_init();
}

void loop()
{
    exti_update();
    oled_update();
    wifi_mgr_update();

    // ------ KEY1: 自锁开关 ------
    if (key1_edge) {
        key1_edge = 0;
        if (key1_is_on()) {
            power_on();
        } else {
            power_off();
        }
    }

    if (!powerOn) {
        key2_holding = false;
        return;
    }

    unsigned long now = millis();

    // ------ KEY2 按下：记录时间 ------
    if (key2_edge) {
        key2_edge = 0;
        key2_down_time = now;
        key2_holding = true;
    }

    // ------ KEY2 长按检测：切换自动/手动 ------
    if (key2_holding) {
        unsigned long hold = now - key2_down_time;
        if (hold >= LONG_PRESS_MIN && hold <= LONG_PRESS_MAX) {
            toggle_auto();
            key2_holding = false;
        }
    }

    // ------ KEY2 松手检测：短按（仅手动模式）------
    static bool last_key2 = false;
    bool now_key2 = (digitalRead(KEY2_PIN) == HIGH);
    if (last_key2 && !now_key2 && key2_holding) {
        unsigned long hold = now - key2_down_time;
        if (hold < LONG_PRESS_MIN) {
            if (!autoMode) {
                set_mode(mode % 4 + 1);
            } else {
                Serial.println("In AUTO mode, short press disabled");
            }
        }
        key2_holding = false;
    }
    last_key2 = now_key2;

    // ------ 自动模式：ADC + 温度控制 ------
    if (autoMode) {
        oled_read_sensors();
        if (oled_get_voltage() > 2.0f) {
            led_on();
        } else {
            led_off();
        }

        if (oled_get_temp() > TEMP_THRESHOLD) {
            fan_on();
        } else {
            fan_off();
        }

        delay(50);
    }
}
