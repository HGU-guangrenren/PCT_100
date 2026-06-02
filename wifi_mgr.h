#ifndef __WIFI_MGR_H
#define __WIFI_MGR_H

#include "Arduino.h"

// ============================================================================
// WiFi 配网模式选择 (在 PCT_100.ino 里也可以再次 -D 覆盖)
//   WIFI_MODE_STATIC = 1   初步要求: 写死账号密码, 上电自动连接
//   WIFI_MODE_SCAN   = 2   升级要求: 扫描附近WiFi, 串口选择 + 输入密码
//   WIFI_MODE_SAVED  = 3   高级要求: 在模式2基础上, 连接成功后保存到 Flash,
//                                下次上电优先用保存的账号直接联网
// ============================================================================
#define WIFI_MODE_STATIC    1
#define WIFI_MODE_SCAN      2
#define WIFI_MODE_SAVED     3

#ifndef WIFI_MODE
#define WIFI_MODE   WIFI_MODE_SAVED
#endif

// 仅 WIFI_MODE == 1 时使用, 其它模式可忽略
#ifndef WIFI_SSID
#define WIFI_SSID       "TP-LINK_XXXX"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD   "12345678"
#endif

// NVS 命名空间与键名 (Flash 持久化)
#define WIFI_NVS_NS      "pct100"
#define WIFI_NVS_KEY_SSID  "ssid"
#define WIFI_NVS_KEY_PASS  "pass"

// 初始化 (在 setup() 里调用一次)
void wifi_mgr_init(void);

// 周期调用 (在 loop() 里调用)
void wifi_mgr_update(void);

// 状态查询
bool           wifi_mgr_is_connected(void);   // 是否已连接
String         wifi_mgr_get_ip(void);          // IP 字符串, 未连接返回 0.0.0.0
String         wifi_mgr_get_ssid(void);        // 已连接返回 SSID, 否则空
int            wifi_mgr_get_rssi(void);        // 已连接返回信号强度 dBm
const char*    wifi_mgr_get_state_str(void);   // 状态文字: 扫描中/选择/连接中/已连接/...
bool           wifi_mgr_is_provisioning(void); // 是否处于配网等待输入状态

// 控制接口
void wifi_mgr_clear_saved(void);   // 清除 Flash 中保存的账号密码
void wifi_mgr_force_rescan(void);  // 强制重新扫描
void wifi_mgr_reconnect_saved(void); // 读 Flash 并重连 (供 KEY1 ON 调用)

#endif
