#include "mqtt_mgr.h"
#include "oled.h"
#include "wifi_mgr.h"
#include "exti.h"
#include "led.h"
#include "console.h"
#include <ArduinoJson.h>
#include <Preferences.h>

// 来自 PCT_100.ino 的全局状态/控制接口
extern bool get_power_on(void);
extern bool get_auto_mode(void);
extern int  get_mode(void);
extern void toggle_auto(void);

// ============================================================================
// 内部状态
// ============================================================================
static WiFiClient      s_wifi_client;
static PubSubClient    s_mqtt(s_wifi_client);
static char            s_status_topic[MQTT_TOPIC_LEN];
static char            s_command_topic[MQTT_TOPIC_LEN];
static char            s_lwt_topic[MQTT_TOPIC_LEN];
static unsigned long   s_last_publish = 0;
static unsigned long   s_last_connect_attempt = 0;
static bool            s_initialized  = false;
static bool            s_was_wifi_connected = false;

// ============================================================================
// 配置变量 (运行期可变, NVS 持久化)
// ============================================================================
static char     s_ip[MQTT_IP_LEN]            = MQTT_IP_DEFAULT;
static uint16_t s_port                      = MQTT_PORT_DEFAULT;
static char     s_user[MQTT_USER_LEN]        = MQTT_USER_DEFAULT;
static char     s_pass[MQTT_PASS_LEN]        = MQTT_PASS_DEFAULT;
static char     s_device_id[MQTT_ID_LEN]     = DEVICE_ID_DEFAULT;

// NVS 键
static const char* NVS_KEY_IP    = "mqtt_ip";
static const char* NVS_KEY_PORT  = "mqtt_port";
static const char* NVS_KEY_USER  = "mqtt_user";
static const char* NVS_KEY_PASS  = "mqtt_pass";
static const char* NVS_KEY_ID    = "mqtt_id";

// ============================================================================
// NVS 加载 / 保存 (5 项一次性原子写, 防止部分写入)
// ============================================================================
static void load_mqtt_config(void)
{
    Preferences p;
    p.begin("pct100", true);
    String ip   = p.getString(NVS_KEY_IP,   MQTT_IP_DEFAULT);
    int     port = p.getInt(NVS_KEY_PORT,    MQTT_PORT_DEFAULT);
    String user = p.getString(NVS_KEY_USER, MQTT_USER_DEFAULT);
    String pass = p.getString(NVS_KEY_PASS, MQTT_PASS_DEFAULT);
    String id   = p.getString(NVS_KEY_ID,   DEVICE_ID_DEFAULT);
    p.end();

    strncpy(s_ip, ip.c_str(), sizeof(s_ip) - 1);
    s_ip[sizeof(s_ip) - 1] = '\0';
    s_port = (uint16_t)port;
    strncpy(s_user, user.c_str(), sizeof(s_user) - 1);
    s_user[sizeof(s_user) - 1] = '\0';
    strncpy(s_pass, pass.c_str(), sizeof(s_pass) - 1);
    s_pass[sizeof(s_pass) - 1] = '\0';
    strncpy(s_device_id, id.c_str(), sizeof(s_device_id) - 1);
    s_device_id[sizeof(s_device_id) - 1] = '\0';

    Serial.printf("[MQTT] NVS 加载: %s:%d user=%s id=%s\n",
                  s_ip, s_port, s_user, s_device_id);
}

static void save_mqtt_config(void)
{
    Preferences p;
    p.begin("pct100", false);
    p.putString(NVS_KEY_IP,   s_ip);
    p.putInt(NVS_KEY_PORT,    s_port);
    p.putString(NVS_KEY_USER, s_user);
    p.putString(NVS_KEY_PASS, s_pass);
    p.putString(NVS_KEY_ID,   s_device_id);
    p.end();
    Serial.println("[MQTT] 已写入 NVS");
}

// ============================================================================
// 主题构造 (用 s_device_id 拼)
// ============================================================================
static void build_topics(void)
{
    snprintf(s_status_topic,  sizeof(s_status_topic),
             "chemctrl/%s/status",  s_device_id);
    snprintf(s_command_topic, sizeof(s_command_topic),
             "chemctrl/%s/command", s_device_id);
    snprintf(s_lwt_topic,     sizeof(s_lwt_topic),
             "chemctrl/%s/lwt",     s_device_id);
}

// ============================================================================
// publish_status (10 字段)
// ============================================================================
void mqtt_mgr_publish_status(void)
{
    if (!s_mqtt.connected()) return;

    StaticJsonDocument<256> doc;
    doc["temperature"]     = oled_get_temp();
    doc["light"]           = oled_get_lux();
    doc["mode"]            = get_auto_mode() ? "auto" : "manual";
    doc["key1_lock"]       = key1_is_on();
    doc["relay3"]          = led_state();
    doc["relay4"]          = fan_state();
    doc["temp_threshold"]  = g_temp_threshold;
    doc["light_threshold"] = g_light_threshold;

    char buf[256];
    serializeJson(doc, buf, sizeof(buf));
    s_mqtt.publish(s_status_topic, buf);
}

// ============================================================================
// 命令处理 (5 种 cmd, 来自 broker 的下行 topic)
// ============================================================================
static void handle_set_relay(JsonDocument& doc)
{
    if (!get_power_on()) {
        Serial.println("[MQTT] set_relay 拒绝: KEY1 OFF");
        return;
    }
    int  relay = doc["relay"] | 0;
    bool val   = doc["value"] | false;

    if (relay == 3) {
        if (val) led_on(); else led_off();
        Serial.printf("[MQTT] set_relay relay3=%s\n", val ? "on" : "off");
    } else if (relay == 4) {
        if (val) fan_on(); else fan_off();
        Serial.printf("[MQTT] set_relay relay4=%s\n", val ? "on" : "off");
    } else {
        Serial.printf("[MQTT] set_relay relay=%d 不支持 (仅 3/4)\n", relay);
        return;
    }
    mqtt_mgr_publish_status();
}

static void handle_get_status(void)
{
    Serial.println("[MQTT] get_status");
    mqtt_mgr_publish_status();
}

static void handle_set_mode(JsonDocument& doc)
{
    const char* mode = doc["mode"] | "";
    if (strcmp(mode, "auto") == 0) {
        if (!get_auto_mode()) toggle_auto();
        Serial.println("[MQTT] set_mode auto");
    } else if (strcmp(mode, "manual") == 0) {
        if (get_auto_mode()) toggle_auto();
        Serial.println("[MQTT] set_mode manual");
    } else {
        Serial.printf("[MQTT] set_mode 未知: %s\n", mode);
        return;
    }
    mqtt_mgr_publish_status();
}

static void handle_set_threshold(JsonDocument& doc)
{
    bool changed = false;
    if (doc.containsKey("temp")) {
        float t = doc["temp"].as<float>();
        oled_set_temp_threshold(t);
        Serial.printf("[MQTT] set_threshold temp=%.1f\n", t);
        changed = true;
    }
    if (doc.containsKey("light")) {
        int l = doc["light"].as<int>();
        oled_set_light_threshold(l);
        Serial.printf("[MQTT] set_threshold light=%d\n", l);
        changed = true;
    }
    if (changed) mqtt_mgr_publish_status();
}

static void handle_reboot(void)
{
    Serial.println("[MQTT] reboot");
    mqtt_mgr_publish_status();
    delay(1000);
    ESP.restart();
}

// ============================================================================
// MQTT 消息回调
// ============================================================================
static void mqtt_callback(char* topic, byte* payload, unsigned int len)
{
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, payload, len);
    if (err) {
        Serial.printf("[MQTT] JSON 解析失败: %s\n", err.c_str());
        return;
    }

    const char* cmd = doc["cmd"] | "";
    Serial.printf("[MQTT] cmd=%s\n", cmd);

    if      (strcmp(cmd, "set_relay")     == 0) handle_set_relay(doc);
    else if (strcmp(cmd, "get_status")    == 0) handle_get_status();
    else if (strcmp(cmd, "set_mode")      == 0) handle_set_mode(doc);
    else if (strcmp(cmd, "set_threshold") == 0) handle_set_threshold(doc);
    else if (strcmp(cmd, "reboot")        == 0) handle_reboot();
    else Serial.printf("[MQTT] 未知 cmd: %s\n", cmd);
}

// ============================================================================
// 连接 broker
// ============================================================================
static bool try_connect(void)
{
    String clientId = String(s_device_id);
    Serial.printf("[MQTT] 连接 %s:%d clientId=%s user=%s\n",
                  s_ip, s_port, clientId.c_str(), s_user);

    bool ok = s_mqtt.connect(
        clientId.c_str(),
        s_user, s_pass,
        s_lwt_topic, MQTT_QOS, true,
        MQTT_LWT_OFFLINE
    );

    if (!ok) {
        Serial.printf("[MQTT] 连接失败, rc=%d\n", s_mqtt.state());
        s_last_connect_attempt = millis();
        return false;
    }

    Serial.println("[MQTT] 连接成功");
    s_mqtt.subscribe(s_command_topic, MQTT_QOS);
    Serial.printf("[MQTT] 订阅: %s\n", s_command_topic);

    s_mqtt.publish(s_lwt_topic, MQTT_LWT_ONLINE, true);
    Serial.printf("[MQTT] LWT online -> %s\n", s_lwt_topic);

    s_last_publish = millis();
    s_last_connect_attempt = millis();
    mqtt_mgr_publish_status();
    return true;
}

// 配置被改 / 强制重连后, 让下一次 update() 立即重试 (不受 5s 间隔约束)
static void reset_retry_timer(void)
{
    s_last_connect_attempt = 0;
}

// ============================================================================
// 公开 API: init / update / is_connected
// ============================================================================
void mqtt_mgr_init(void)
{
    load_mqtt_config();
    build_topics();
    s_mqtt.setServer(s_ip, s_port);
    s_mqtt.setKeepAlive(MQTT_KEEPALIVE_S);
    s_mqtt.setCallback(mqtt_callback);
    s_initialized = true;
    Serial.println("[MQTT] init");
    Serial.printf("[MQTT] status=%s\n",   s_status_topic);
    Serial.printf("[MQTT] command=%s\n",  s_command_topic);
    Serial.printf("[MQTT] lwt=%s\n",      s_lwt_topic);
}

void mqtt_mgr_update(void)
{
    if (!s_initialized) return;

    bool wifi_ok = wifi_mgr_is_connected();

    // WiFi 刚恢复: 立即尝试一次 MQTT 连接 (不等待 5s 重试间隔)
    if (wifi_ok && !s_was_wifi_connected) {
        s_was_wifi_connected = true;
        if (!s_mqtt.connected()) {
            s_last_connect_attempt = millis();
            try_connect();
            return;
        }
    } else if (!wifi_ok) {
        s_was_wifi_connected = false;
    }

    if (!wifi_ok) {
        if (s_mqtt.connected()) {
            Serial.println("[MQTT] WiFi 断开, MQTT 断开");
            s_mqtt.disconnect();
        }
        return;
    }

    if (!s_mqtt.connected()) {
        unsigned long now = millis();
        if (now - s_last_connect_attempt > MQTT_RETRY_INTERVAL_MS) {
            s_last_connect_attempt = now;
            try_connect();
        }
        return;
    }

    s_mqtt.loop();

    unsigned long now = millis();
    if (now - s_last_publish > MQTT_HEARTBEAT_MS) {
        mqtt_mgr_publish_status();
        s_last_publish = now;
    }
}

bool mqtt_mgr_is_connected(void)
{
    return s_initialized && s_mqtt.connected();
}

// ============================================================================
// 配置 setter
// ============================================================================
void mqtt_mgr_set_ip(const char* ip)
{
    if (!ip || strlen(ip) == 0) {
        Serial.println("[MQTT] IP 为空, 拒绝");
        return;
    }
    strncpy(s_ip, ip, sizeof(s_ip) - 1);
    s_ip[sizeof(s_ip) - 1] = '\0';
    save_mqtt_config();
    Serial.printf("[MQTT] IP -> %s\n", s_ip);
    s_mqtt.setServer(s_ip, s_port);
    if (s_mqtt.connected()) s_mqtt.disconnect();
    reset_retry_timer();
}

void mqtt_mgr_set_port(uint16_t port)
{
    if (port == 0) {
        Serial.println("[MQTT] PORT=0 非法, 拒绝");
        return;
    }
    s_port = port;
    save_mqtt_config();
    Serial.printf("[MQTT] PORT -> %u\n", s_port);
    s_mqtt.setServer(s_ip, s_port);
    if (s_mqtt.connected()) s_mqtt.disconnect();
    reset_retry_timer();
}

void mqtt_mgr_set_user(const char* user)
{
    if (!user || strlen(user) == 0) {
        Serial.println("[MQTT] USER 为空, 拒绝");
        return;
    }
    strncpy(s_user, user, sizeof(s_user) - 1);
    s_user[sizeof(s_user) - 1] = '\0';
    save_mqtt_config();
    Serial.printf("[MQTT] USER -> %s\n", s_user);
    if (s_mqtt.connected()) s_mqtt.disconnect();
    reset_retry_timer();
}

void mqtt_mgr_set_pass(const char* pass)
{
    if (!pass) {
        Serial.println("[MQTT] PASS 为空, 拒绝");
        return;
    }
    strncpy(s_pass, pass, sizeof(s_pass) - 1);
    s_pass[sizeof(s_pass) - 1] = '\0';
    save_mqtt_config();
    Serial.printf("[MQTT] PASS -> %s (已保存)\n", s_pass);
    if (s_mqtt.connected()) s_mqtt.disconnect();
    reset_retry_timer();
}

void mqtt_mgr_set_device_id(const char* id)
{
    if (!id || strlen(id) == 0) {
        Serial.println("[MQTT] ID 为空, 拒绝");
        return;
    }
    if (strlen(id) >= MQTT_ID_LEN) {
        Serial.printf("[MQTT] ID 过长 (>=%d), 拒绝\n", MQTT_ID_LEN);
        return;
    }
    strncpy(s_device_id, id, sizeof(s_device_id) - 1);
    s_device_id[sizeof(s_device_id) - 1] = '\0';
    save_mqtt_config();
    Serial.printf("[MQTT] ID -> %s\n", s_device_id);
    build_topics();
    if (s_mqtt.connected()) s_mqtt.disconnect();
    reset_retry_timer();
}

void mqtt_mgr_clear_config(void)
{
    strncpy(s_ip, MQTT_IP_DEFAULT, sizeof(s_ip) - 1);
    s_ip[sizeof(s_ip) - 1] = '\0';
    s_port = MQTT_PORT_DEFAULT;
    strncpy(s_user, MQTT_USER_DEFAULT, sizeof(s_user) - 1);
    s_user[sizeof(s_user) - 1] = '\0';
    strncpy(s_pass, MQTT_PASS_DEFAULT, sizeof(s_pass) - 1);
    s_pass[sizeof(s_pass) - 1] = '\0';
    strncpy(s_device_id, DEVICE_ID_DEFAULT, sizeof(s_device_id) - 1);
    s_device_id[sizeof(s_device_id) - 1] = '\0';
    save_mqtt_config();
    Serial.println("[MQTT] 已恢复默认配置");
    s_mqtt.setServer(s_ip, s_port);
    build_topics();
    if (s_mqtt.connected()) s_mqtt.disconnect();
    reset_retry_timer();
}

void mqtt_mgr_reconnect(void)
{
    Serial.println("[MQTT] 强制重连");
    s_mqtt.setServer(s_ip, s_port);
    if (s_mqtt.connected()) s_mqtt.disconnect();
    reset_retry_timer();
}

// ============================================================================
// 配置 getter
// ============================================================================
const char* mqtt_mgr_get_ip(void)          { return s_ip; }
uint16_t    mqtt_mgr_get_port(void)        { return s_port; }
const char* mqtt_mgr_get_user(void)        { return s_user; }
const char* mqtt_mgr_get_device_id(void)   { return s_device_id; }

// ============================================================================
// 串口命令解析
//   命令:
//     MQTT                          帮助
//     MQTT SHOW                     显示当前配置
//     MQTT SET IP <ip>              设置 IP
//     MQTT SET PORT <port>          设置端口
//     MQTT SET USER <user>          设置用户名
//     MQTT SET PASS <pass>          设置密码
//     MQTT SET ID <device_id>       设置 DEVICE_ID
//     MQTT CLEAR                    恢复默认
//     MQTT RECONNECT                强制重连
// ============================================================================
void mqtt_mgr_console(void)
{
    String line = console_take();
    if (line.length() == 0) return;

    if (!line.startsWith("MQTT")) {
        // 非本模块命令, 退回给下个 console
        console_give_back(line);
        return;
    }

    // "MQTT" 单字 (4 字符) -> 打印帮助
    if (line.length() == 4) {
        Serial.println("[MQTT] 命令 (输入 'MQTT SHOW' 看当前配置):");
        Serial.println("  MQTT SET IP <ip>");
        Serial.println("  MQTT SET PORT <port>");
        Serial.println("  MQTT SET USER <user>");
        Serial.println("  MQTT SET PASS <pass>");
        Serial.println("  MQTT SET ID <device_id>");
        Serial.println("  MQTT SHOW");
        Serial.println("  MQTT CLEAR");
        Serial.println("  MQTT RECONNECT");
        return;
    }

    String arg = line.substring(5);  // 跳过 "MQTT "
    arg.trim();

    if (arg.equalsIgnoreCase("SHOW")) {
        Serial.println("========== MQTT 当前配置 ==========");
        Serial.printf("  IP       : %s\n", s_ip);
        Serial.printf("  PORT     : %u\n", s_port);
        Serial.printf("  USER     : %s\n", s_user);
        Serial.printf("  PASS     : %s\n", s_pass);
        Serial.printf("  DEVICE_ID: %s\n", s_device_id);
        Serial.printf("  状态     : %s\n", s_mqtt.connected() ? "已连接" : "未连接");
        Serial.println("====================================");
    }
    else if (arg.equalsIgnoreCase("CLEAR")) {
        mqtt_mgr_clear_config();
    }
    else if (arg.equalsIgnoreCase("RECONNECT")) {
        mqtt_mgr_reconnect();
    }
    else if (arg.startsWith("SET ") || arg.startsWith("set ")) {
        String kv = arg.substring(4);
        kv.trim();
        int sp = kv.indexOf(' ');
        if (sp < 0) {
            Serial.println("[MQTT] 格式: MQTT SET <KEY> <VAL>");
            Serial.println("  KEY: IP / PORT / USER / PASS / ID");
            return;
        }
        String key = kv.substring(0, sp);
        key.trim();
        key.toUpperCase();
        String val = kv.substring(sp + 1);
        val.trim();

        if (val.length() == 0) {
            Serial.println("[MQTT] 值不能为空");
            return;
        }

        if      (key == "IP")   mqtt_mgr_set_ip(val.c_str());
        else if (key == "PORT") mqtt_mgr_set_port((uint16_t)val.toInt());
        else if (key == "USER") mqtt_mgr_set_user(val.c_str());
        else if (key == "PASS") mqtt_mgr_set_pass(val.c_str());
        else if (key == "ID")   mqtt_mgr_set_device_id(val.c_str());
        else {
            Serial.printf("[MQTT] 未知 KEY '%s', 支持: IP/PORT/USER/PASS/ID\n", key.c_str());
        }
    }
    else {
        Serial.printf("[MQTT] 未知命令 '%s', 输入 'MQTT' 看帮助\n", arg.c_str());
    }
}
