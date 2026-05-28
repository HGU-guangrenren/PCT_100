#include "key.h" 

void key_init(void) 
{ 
    pinMode(KEY1_PIN, INPUT_PULLUP);   
    pinMode(KEY2_PIN, INPUT_PULLUP);   
}