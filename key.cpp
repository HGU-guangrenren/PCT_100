#include "key.h" 

volatile bool key1_state = false;
volatile bool key2_state = false;

void key_init(void) 
{ 
    pinMode(KEY1_PIN, INPUT_PULLUP);   
    pinMode(KEY2_PIN, INPUT_PULLUP);   
}

void key1_isr(void)
{
    static unsigned long last_time = 0;
    unsigned long now = millis();
    if (now - last_time > 200)
    {
        key1_state = !key1_state;
    }
    last_time = now;
}

void key2_isr(void)
{
    static unsigned long last_time = 0;
    unsigned long now = millis();
    if (now - last_time > 200)
    {
        key2_state = !key2_state;
    }
    last_time = now;
}