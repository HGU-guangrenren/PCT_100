#include "led.h"
#include "version.h"

// ------ 软件消抖（通用短按）------
struct Debounce {
    int idle, st, lr;
    unsigned long tc;
    bool ready;
};

static bool isPress(int pin, Debounce &d) {
    int r = digitalRead(pin);
    if (!d.ready) {
        d.idle = r; d.st = r; d.lr = r; d.ready = true;
        return false;
    }
    if (r != d.lr) { d.lr = r; d.tc = millis(); }
    if (millis() - d.tc >= 50 && d.lr != d.st) {
        if (d.st == d.idle && d.lr != d.idle) {
            d.st = d.lr;
            return true;
        }
        d.st = d.lr;
    }
    return false;
}

// ------ KEY2 事件：0=无, 1=短按, 2=长按(2s) ------
static int key2Event(void) {
    static int idle = -1, st, lr;
    static unsigned long tc, pressTime;
    static bool ready = false, longTriggered = false;
    static unsigned long bootTime;

    int r = digitalRead(SW2_PIN);
    if (!ready) {
        idle = r; st = r; lr = r; bootTime = millis(); ready = true;
        return 0;
    }

    unsigned long now = millis();
    if (r != lr) { lr = r; tc = now; }

    // 开机500ms内引脚稳定时同步idle，避免噪声导致误判
    if (lr == st && now - tc > 100 && millis() - bootTime < 500) {
        idle = st;
    }

    if (now - tc >= 50 && lr != st) {
        if (st == idle && lr != idle) {
            st = lr;
            pressTime = now;
            longTriggered = false;
        } else {
            st = lr;
            if (!longTriggered && now - pressTime < 2000) {
                return 1;
            }
        }
    }

    if (lr != idle && !longTriggered && now - pressTime >= 2000) {
        longTriggered = true;
        return 2;
    }

    return 0;
}

static Debounce sw1;
static bool powerOn = false;
static int mode = 0;
static bool autoMode = false;
static int lastManualMode = 4;

// ------ Control API (called from MQTT) ------
void power_on(void)
{
    powerOn = true;
    mode = 4;
    led_off();
    fan_off();
    Serial.println("POWER ON");
}

void power_off(void)
{
    powerOn = false;
    mode = 0;
    led_off();
    fan_off();
    Serial.println("POWER OFF");
}

void set_mode(int m)
{
    if (!powerOn || autoMode) return;
    mode = m;
    switch (mode) {
        case 1: led_on(); fan_off(); Serial.println("Light ON"); break;
        case 2: led_off(); fan_on(); Serial.println("Fan ON"); break;
        case 3: led_on(); fan_on(); Serial.println("Both ON"); break;
        case 4: led_off(); fan_off(); Serial.println("Both OFF"); break;
    }
    lastManualMode = mode;
}

void toggle_auto(void)
{
    if (!powerOn) return;
    autoMode = !autoMode;
    if (autoMode) {
        mode = 4;
        led_off();
        fan_off();
        Serial.println("AUTO MODE - All OFF");
    } else {
        Serial.println("MANUAL MODE");
        mode = lastManualMode;
        switch (mode) {
            case 1: led_on(); fan_off(); break;
            case 2: led_off(); fan_on(); break;
            case 3: led_on(); fan_on(); break;
            case 4: led_off(); fan_off(); break;
        }
    }
}

bool get_power_on(void) { return powerOn; }
int  get_mode(void)     { return mode; }
bool get_auto_mode(void) { return autoMode; }

void setup()
{
    Serial.begin(115200);
    pinMode(6, OUTPUT);
    digitalWrite(6, HIGH);

    for (int i = 0; i < 5; i++) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("--- SYSTEM STARTED ---");

    switch_init();
    led_init();
    fan_init();
    led_off();
    fan_off();
    digitalWrite(6, LOW);

    Serial.println("System initialized!");
    Serial.println("KEY1=Power  KEY2=short:Mode  hold:Auto");
}

void loop()
{
    // ------ KEY1: 自锁开关，直接读物理状态 ------
    {
        static int last = HIGH;
        int cur = digitalRead(SW1_PIN);
        if (cur != last) {
            last = cur;
            if (cur == HIGH) power_on(); else power_off();
        }
    }

    // ------ KEY2: 短按切模式 / 长按切换自动/手动 ------
    int ev = key2Event();
    if (powerOn && ev == 1) {
        if (autoMode) {
            Serial.println("In AUTO mode, short press disabled");
        } else {
            set_mode(mode % 4 + 1);
        }
    }

    if (powerOn && ev == 2) {
        toggle_auto();
    }
}