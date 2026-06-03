#ifndef __MQTT_MGR_H
#define __MQTT_MGR_H

#include "Arduino.h"
#include <WiFi.h>
#include <PubSubClient.h>

// ============================================================================
// MQTT 默认值 (NVS 缺失时使用, 即出厂配置)
//   修改默认值后烧录新固件即可应用
//   旧 NVS 中保存的值优先级更高, 可用 'MQTT CLEAR' 串口命令恢复默认
// ============================================================================
#define MQTT_IP_DEFAULT       "your_broker_ip"
#define MQTT_PORT_DEFAULT     8081
#define MQTT_USER_DEFAULT     "your_username"
#define MQTT_PASS_DEFAULT     "your_password"
#define DEVICE_ID_DEFAULT     "PCT_100_005"

// 字符串缓冲长度
#define MQTT_IP_LEN           32
#define MQTT_USER_LEN         32
#define MQTT_PASS_LEN         32
#define MQTT_ID_LEN           32

// 协议参数
#define MQTT_KEEPALIVE_S      60
#define MQTT_QOS              1

// 慢心跳兜底上报周期 (60s)
#define MQTT_HEARTBEAT_MS     60000UL

// 连接失败后最小重试间隔 (避免端口 RST 场景下日志刷屏)
#define MQTT_RETRY_INTERVAL_MS  5000UL

// LWT payload
#define MQTT_LWT_OFFLINE      "{\"online\":false}"
#define MQTT_LWT_ONLINE       "{\"online\":true}"

// topic 缓冲长度
#define MQTT_TOPIC_LEN        64

// ============================================================================
// 公开 API
// ============================================================================
void   mqtt_mgr_init(void);
void   mqtt_mgr_update(void);
void   mqtt_mgr_console(void);
bool   mqtt_mgr_is_connected(void);
void   mqtt_mgr_publish_status(void);

// 配置 setter (串口命令 / 远程调用)
void   mqtt_mgr_set_ip(const char* ip);
void   mqtt_mgr_set_port(uint16_t port);
void   mqtt_mgr_set_user(const char* user);
void   mqtt_mgr_set_pass(const char* pass);
void   mqtt_mgr_set_device_id(const char* id);
void   mqtt_mgr_clear_config(void);
void   mqtt_mgr_reconnect(void);

// 配置 getter
const char* mqtt_mgr_get_ip(void);
uint16_t    mqtt_mgr_get_port(void);
const char* mqtt_mgr_get_user(void);
const char* mqtt_mgr_get_device_id(void);

#endif
