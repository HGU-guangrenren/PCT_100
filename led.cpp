#include "led.h"

void led_init(void)
{
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LED_OFF);
}

void led_on(void)
{
    digitalWrite(LED_PIN, LED_ON);
}

void led_off(void)
{
    digitalWrite(LED_PIN, LED_OFF);
}

void led_toggle(void)
{
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
}

void fan_init(void)
{
    pinMode(FAN_PIN, OUTPUT);
    digitalWrite(FAN_PIN, FAN_OFF);
}

void fan_on(void)
{
    digitalWrite(FAN_PIN, FAN_ON);
}

void fan_off(void)
{
    digitalWrite(FAN_PIN, FAN_OFF);
}

void fan_toggle(void)
{
    digitalWrite(FAN_PIN, !digitalRead(FAN_PIN));
}

