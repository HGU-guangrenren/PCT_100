#include "led.h"
#include "version.h"

// ------ 软件消抖（来自 gpio_demo）------
struct Debounce {
    int idle, st, lr;
    unsigned long tc;
    bool ready;
};

static bool isPress(int pin, Debounce &d) {
    int r = digitalRead(pin);
    if (!d.ready) {
        d.idle = r; d.st = r; d.lr = r; d.ready = true;
        return false;
    }
    if (r != d.lr) { d.lr = r; d.tc = millis(); }
    if (millis() - d.tc >= 50 && d.lr != d.st) {
        if (d.st == d.idle && d.lr != d.idle) {
            d.st = d.lr;
            return true;
        }
        d.st = d.lr;
    }
    return false;
}

static Debounce sw1, sw2;

void setup()
{
    Serial.begin(115200);
    delay(100);

    switch_init();
    led_init();
    fan_init();
    fan_off();
    led_on();

    Serial.println("System initialized!");
    Serial.println("SW1->LED, SW2->FAN");
}

void loop()
{
    if (isPress(SW1_PIN, sw1)) {
        led_toggle();
        Serial.println(digitalRead(LED_PIN) ? "LED ON" : "LED OFF");
    }

    if (isPress(SW2_PIN, sw2)) {
        fan_toggle();
        Serial.println(digitalRead(FAN_PIN) ? "FAN ON" : "FAN OFF");
    }
}