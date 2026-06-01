#ifndef __EXTI_H
#define __EXTI_H

#include "Arduino.h"
#include "key.h"

extern volatile int key1_edge;
extern volatile int key2_edge;

void exti_init(void);
void exti_update(void);
int  key1_is_on(void);

#endif
