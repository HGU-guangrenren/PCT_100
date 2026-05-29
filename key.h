#ifndef __KEY_H 
#define __KEY_H 

#include "Arduino.h" 

#define KEY1_PIN       10  
#define KEY2_PIN       9  

extern volatile bool key1_state;
extern volatile bool key2_state;

void key_init(void);
void key1_isr(void);
void key2_isr(void);

#endif