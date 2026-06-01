#ifndef __DS18B20_H
#define __DS18B20_H

#include "Arduino.h"
#include <OneWire.h>
#include <DallasTemperature.h>

#define DS18B20_PIN   10

void ds18b20_init(void);
float ds18b20_read_temp(void);

#endif
