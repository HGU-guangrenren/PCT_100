#include "exti.h"
#include "key.h"

void exti_init(void)
{
    key_init();
    
    attachInterrupt(digitalPinToInterrupt(KEY1_PIN), key1_isr, FALLING);
    attachInterrupt(digitalPinToInterrupt(KEY2_PIN), key2_isr, FALLING);
}