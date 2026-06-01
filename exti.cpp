#include "exti.h"

#define STATE_IDLE       0
#define STATE_DEBOUNCE   1
#define STATE_CONFIRMED  2

static uint8_t      k1_state = STATE_IDLE;
static unsigned long k1_time = 0;
static int          k1_stable = 0;
volatile int        key1_edge = 0;

static uint8_t      k2_state = STATE_IDLE;
static unsigned long k2_time = 0;
volatile int        key2_edge = 0;

static void IRAM_ATTR k1_isr(void)
{
    if (k1_state != STATE_IDLE) return;
    k1_state = STATE_DEBOUNCE;
    k1_time  = millis();
}

static void IRAM_ATTR k2_isr(void)
{
    if (k2_state != STATE_IDLE) return;
    k2_state = STATE_DEBOUNCE;
    k2_time  = millis();
}

void exti_init(void)
{
    key_init();

    attachInterrupt(digitalPinToInterrupt(KEY1_PIN), k1_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(KEY2_PIN), k2_isr, RISING);

    k1_stable = digitalRead(KEY1_PIN);
}

void exti_update(void)
{
    unsigned long now = millis();

    if (k1_state == STATE_DEBOUNCE && (now - k1_time >= DEBOUNCE_MS)) {
        int raw = digitalRead(KEY1_PIN);
        if (raw != k1_stable) {
            k1_stable = raw;
            key1_edge = 1;
        }
        k1_state = STATE_IDLE;
    }

    if (k2_state == STATE_DEBOUNCE && (now - k2_time >= DEBOUNCE_MS)) {
        if (digitalRead(KEY2_PIN) == HIGH) {
            key2_edge = 1;
            k2_state  = STATE_CONFIRMED;
        } else {
            k2_state = STATE_IDLE;
        }
    }

    if (k2_state == STATE_CONFIRMED && digitalRead(KEY2_PIN) == LOW) {
        k2_state = STATE_IDLE;
    }
}

int key1_is_on(void)
{
    return (k1_stable == HIGH);
}
