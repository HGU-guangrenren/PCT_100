#ifndef __EXTI_H
#define __EXTI_H

#include "Arduino.h"

extern volatile bool key1_state;
extern volatile bool key2_state;

void exti_init(void);

#endif