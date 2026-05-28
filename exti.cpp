#include "exti.h"
#include "key.h"

volatile bool key1_state = false;
volatile bool key2_state = false;

void exti_init(void)
{
    key_init();
    
    attachInterrupt(digitalPinToInterrupt(KEY1_PIN), key1_isr, FALLING);
    attachInterrupt(digitalPinToInterrupt(KEY2_PIN), key2_isr, FALLING);
}

void key1_isr(void)
{
    static unsigned long last_interrupt_time1 = 0;
    unsigned long interrupt_time1 = millis();
    
    if (interrupt_time1 - last_interrupt_time1 > 200)
    {
        if (digitalRead(KEY1_PIN) == LOW)
        {
            key1_state = !key1_state;
        }
    }
    last_interrupt_time1 = interrupt_time1;
}

void key2_isr(void)
{
    static unsigned long last_interrupt_time2 = 0;
    unsigned long interrupt_time2 = millis();
    
    if (interrupt_time2 - last_interrupt_time2 > 200)
    {
        if (digitalRead(KEY2_PIN) == LOW)
        {
            key2_state = !key2_state;
        }
    }
    last_interrupt_time2 = interrupt_time2;
}