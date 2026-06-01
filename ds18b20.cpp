#include "ds18b20.h"

static OneWire oneWire(DS18B20_PIN);
static DallasTemperature sensors(&oneWire);

void ds18b20_init(void)
{
    pinMode(DS18B20_PIN, INPUT_PULLUP);
    sensors.begin();

    Serial.print("DS18B20 设备数: ");
    Serial.println(sensors.getDeviceCount());

    DeviceAddress addr;
    if (sensors.getAddress(addr, 0)) {
        Serial.print("传感器地址: ");
        for (uint8_t i = 0; i < 8; i++) {
            if (addr[i] < 0x10) Serial.print("0");
            Serial.print(addr[i], HEX);
        }
        Serial.println();
    } else {
        Serial.println("未找到传感器地址");
    }
}

float ds18b20_read_temp(void)
{
    sensors.requestTemperatures();
    float temp = sensors.getTempCByIndex(0);

    Serial.print("DS18B20 温度: ");
    Serial.print(temp);
    Serial.println(" °C");

    return temp;
}
