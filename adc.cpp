#include "adc.h"

void adc_init(void)
{
    pinMode(ADC_PIN, INPUT);
}

float adc_read_voltage(void)
{
    int adcValue = analogRead(ADC_PIN);
    float voltage = adcValue * (3.3 / 4095.0);

    Serial.print("ADC 值: ");
    Serial.print(adcValue);
    Serial.print("  |  电压: ");
    Serial.print(voltage);
    Serial.println(" V");

    return voltage;
}

int adc_read_raw(void)
{
    return analogRead(ADC_PIN);
}


