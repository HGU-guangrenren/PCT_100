#ifndef __KEY_H 
#define __KEY_H 

#include "Arduino.h" 
#define KEY_PIN       21  
#define KEY           digitalRead(KEY_PIN)  
void key_init(void);     

#endif