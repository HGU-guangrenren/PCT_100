#include "key.h"
#include "relay.h"
void setup()
{
    key_init();
    Serial.begin(9600);
}

void loop()
{
    if (KEY == LOW)
    {
        delay(10);
        if (KEY == LOW)
        {
            Serial.println("Key pressed!");
            while (KEY == LOW);
        }
    }
}