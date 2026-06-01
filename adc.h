#ifndef __ADC_H
#define __ADC_H

#include "Arduino.h"

#define ADC_PIN       1

void adc_init(void);
float adc_read_voltage(void);

#endif
