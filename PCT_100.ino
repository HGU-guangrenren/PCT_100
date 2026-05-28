#include "key.h"
#include "exti.h"
#include "version.h"

void setup()
{
    exti_init();
    Serial.begin(115200);
    Serial.println("System initialized!");
    Serial.println("KEY1 toggles LED1, KEY2 toggles LED2");
}

void loop()
{
    static bool last_key1_state = false;
    static bool last_key2_state = false;
    
    if (key1_state != last_key1_state)
    {
        Serial.print("KEY1: ");
        Serial.println(key1_state ? "ON" : "OFF");
        last_key1_state = key1_state;
    }
    
    if (key2_state != last_key2_state)
    {
        Serial.print("KEY2: ");
        Serial.println(key2_state ? "ON" : "OFF");
        last_key2_state = key2_state;
    }
}