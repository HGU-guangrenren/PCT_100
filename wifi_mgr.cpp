#include "wifi_mgr.h"
#include <WiFi.h>
#include <Preferences.h>

// ============================================================================
// 状态机
// ============================================================================
enum WifiState {
    WFS_IDLE = 0,        // 空闲
    WFS_SCANNING,        // 异步扫描中
    WFS_SCAN_WAIT,       // 扫描完0个网络, 等待 3 秒重试
    WFS_SELECT,          // 等待用户输入序号
    WFS_PASSWORD,        // 等待用户输入密码
    WFS_CONNECTING,      // 正在连接
    WFS_CONNECTED,       // 已连接 (同时维持后台重连)
};

static WifiState      s_state        = WFS_IDLE;
static unsigned long  s_state_enter  = 0;
static unsigned long  s_conn_start   = 0;
static int            s_net_count    = 0;
static String         s_sel_ssid;        // 本次选中的 SSID
static String         s_conn_pass;       // 本次连接的密码 (用于保存到 Flash)
static String         s_line;            // 串口行输入缓冲
static bool           s_should_save  = false;  // 连接成功是否写入 Flash

static Preferences    s_nvs;

static const unsigned long CONNECT_TIMEOUT_MS   = 20000;  // 单次连接超时
static const unsigned long SCAN_TIMEOUT_MS     = 15000;  // 扫描超时

// ============================================================================
// 工具函数
// ============================================================================
static const char* state_name(WifiState s)
{
    switch (s) {
        case WFS_IDLE:        return "IDLE";
        case WFS_SCANNING:    return "SCANNING";
        case WFS_SCAN_WAIT:   return "SCAN_WAIT";
        case WFS_SELECT:      return "SELECT";
        case WFS_PASSWORD:    return "PASSWORD";
        case WFS_CONNECTING:  return "CONNECTING";
        case WFS_CONNECTED:   return "CONNECTED";
    }
    return "?";
}

static const char* enc_name(wifi_auth_mode_t e)
{
    switch (e) {
        case WIFI_AUTH_OPEN:            return "开放";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA-PSK";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2-PSK";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3-PSK";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
        default:                        return "加密";
    }
}

static void load_from_flash(String& ssid, String& pass)
{
    s_nvs.begin(WIFI_NVS_NS, true);  // 只读
    ssid = s_nvs.getString(WIFI_NVS_KEY_SSID, "");
    pass = s_nvs.getString(WIFI_NVS_KEY_PASS, "");
    s_nvs.end();
}

static void save_to_flash(const String& ssid, const String& pass)
{
    s_nvs.begin(WIFI_NVS_NS, false);
    s_nvs.putString(WIFI_NVS_KEY_SSID, ssid);
    s_nvs.putString(WIFI_NVS_KEY_PASS, pass);
    s_nvs.end();
    Serial.printf("[WiFi] >>> 已保存到 Flash: SSID=%s (下次上电自动连接)\r\n", ssid.c_str());
}

static void clear_flash(void)
{
    s_nvs.begin(WIFI_NVS_NS, false);
    s_nvs.remove(WIFI_NVS_KEY_SSID);
    s_nvs.remove(WIFI_NVS_KEY_PASS);
    s_nvs.end();
}

static void start_connect(const String& ssid, const String& pass)
{
    Serial.printf("\r\n[WiFi] >>> 正在连接 SSID=\"%s\" ...\r\n", ssid.c_str());
    s_sel_ssid  = ssid;
    s_conn_pass = pass;
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(50);
    WiFi.begin(ssid.c_str(), pass.c_str());
    s_conn_start = millis();
    s_state      = WFS_CONNECTING;
}

static void print_scan_result(void)
{
    s_net_count = WiFi.scanComplete();
    if (s_net_count < 0) s_net_count = 0;

    Serial.println();
    Serial.printf("[WiFi] 扫描完成, 共发现 %d 个网络:\r\n", s_net_count);
    if (s_net_count == 0) {
        Serial.println("[WiFi] (无网络, 3 秒后自动重试)");
    } else {
        Serial.println("    序号  信号       加密类型    SSID");
        Serial.println("    ----  ---------  ----------  ----------------");
        for (int i = 0; i < s_net_count; i++) {
            Serial.printf("    %3d   %4d dBm  %-10s  %s\r\n",
                i, WiFi.RSSI(i), enc_name(WiFi.encryptionType(i)),
                WiFi.SSID(i).c_str());
        }
    }
    Serial.println();
    Serial.printf("[WiFi] 请输入要连接的序号 (0 ~ %d): ", s_net_count - 1);
}

static void start_scan(void)
{
    Serial.println("[WiFi] 正在扫描附近网络...");
    WiFi.scanDelete();
    WiFi.scanNetworks(/*async=*/true);
    s_state       = WFS_SCANNING;
    s_state_enter = millis();
}

// ============================================================================
// 串口行输入处理
// ============================================================================
static void poll_serial(void)
{
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\r') continue;
        if (c == '\n') {
            String line = s_line;
            s_line = "";
            line.trim();

            // --- 全局命令 (任何状态下都生效) ---
            String up = line;
            up.toUpperCase();
            if (up == "RESET" || up == "CLEAR") {
                clear_flash();
                Serial.println("[WiFi] 已清除 Flash 中保存的账号密码, 请重启设备");
                continue;
            }
            if (up == "STATUS") {
                Serial.printf("[WiFi] 状态=%s  IP=%s  SSID=%s  RSSI=%d dBm\r\n",
                    state_name(s_state),
                    wifi_mgr_get_ip().c_str(),
                    wifi_mgr_get_ssid().c_str(),
                    wifi_mgr_get_rssi());
                if (s_state == WFS_SELECT) {
                    Serial.printf("[WiFi] 等待输入序号 (0~%d): ", s_net_count - 1);
                } else if (s_state == WFS_PASSWORD) {
                    Serial.print("[WiFi] 等待输入密码: ");
                }
                continue;
            }
            if (up == "SCAN" || up == "RESCAN") {
                if (s_state != WFS_CONNECTING) {
                    start_scan();
                } else {
                    Serial.println("[WiFi] 正在连接中, 暂不能重扫");
                }
                continue;
            }
            if (up == "HELP" || up == "?") {
                Serial.println("[WiFi] 命令:");
                Serial.println("  SCAN/RESCAN   重新扫描");
                Serial.println("  STATUS        查看状态/IP/SSID/RSSI");
                Serial.println("  DISCONNECT/OFF  断开当前 WiFi 连接");
                Serial.println("  RECONNECT/RC    重新连接 (模式3读Flash, 其它重新扫描)");
                Serial.println("  RESET/CLEAR    清除Flash中保存的账号");
                continue;
            }
            if (up == "DISCONNECT" || up == "OFF" || up == "DC") {
                if (WiFi.status() == WL_CONNECTED) {
                    Serial.printf("[WiFi] 断开连接: SSID=%s IP=%s\r\n",
                        WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
                } else {
                    Serial.println("[WiFi] 当前未连接");
                }
                WiFi.disconnect();
                s_state = WFS_IDLE;
                Serial.println("[WiFi] 已断开, 输入 RECONNECT 重新连接, 或 SCAN 重新配网");
                continue;
            }
            if (up == "RECONNECT" || up == "RC") {
                if (s_state == WFS_CONNECTING) {
                    Serial.println("[WiFi] 正在连接中, 请稍候");
                    continue;
                }
                WiFi.disconnect();
                delay(50);
                Serial.println("[WiFi] >>> 重新连接...");
#if WIFI_MODE == WIFI_MODE_STATIC
                s_should_save = true;
                start_connect(String(WIFI_SSID), String(WIFI_PASSWORD));
#elif WIFI_MODE == WIFI_MODE_SAVED
                String ssid, pass;
                load_from_flash(ssid, pass);
                if (ssid.length() > 0) {
                    s_should_save = true;
                    start_connect(ssid, pass);
                } else {
                    s_should_save = true;
                    start_scan();
                }
#else
                start_scan();
#endif
                continue;
            }

            // --- 状态相关输入 ---
            if (s_state == WFS_SELECT) {
                int idx = line.toInt();
                if (idx < 0 || idx >= s_net_count) {
                    Serial.printf("[WiFi] 序号无效, 请重新输入 (0~%d): ", s_net_count - 1);
                } else {
                    s_sel_ssid = WiFi.SSID(idx);
                    Serial.printf("[WiFi] 已选择: [%d] \"%s\"  信号=%d dBm  加密=%s\r\n",
                        idx, s_sel_ssid.c_str(),
                        WiFi.RSSI(idx), enc_name(WiFi.encryptionType(idx)));
                    if (WiFi.encryptionType(idx) == WIFI_AUTH_OPEN) {
                        Serial.println("[WiFi] 开放网络, 直接连接...");
                        start_connect(s_sel_ssid, "");
                    } else {
                        Serial.print("[WiFi] 请输入密码: ");
                        s_state = WFS_PASSWORD;
                    }
                }
            }
            else if (s_state == WFS_PASSWORD) {
                start_connect(s_sel_ssid, line);
            }
            else if (line.length() > 0) {
                Serial.printf("[WiFi] 当前状态=%s, 无可用输入 (HELP 查看命令)\r\n",
                    state_name(s_state));
            }
        }
        else if (c == 0x08 || c == 0x7F) {  // 退格
            if (s_line.length() > 0) s_line.remove(s_line.length() - 1);
        }
        else if (c >= 0x20 && c <= 0x7E) {  // 可打印字符
            if (s_line.length() < 64) s_line += c;
        }
    }
}

// ============================================================================
// 公开接口
// ============================================================================
void wifi_mgr_init(void)
{
    Serial.println();
    Serial.println("============================================");
    Serial.println("          WiFi 配网模块 初始化");
#if WIFI_MODE == WIFI_MODE_STATIC
    Serial.println("          模式: 静态账号密码 (1)");
#elif WIFI_MODE == WIFI_MODE_SCAN
    Serial.println("          模式: 扫描 + 串口选择 (2)");
#elif WIFI_MODE == WIFI_MODE_SAVED
    Serial.println("          模式: 保存自动连接 (3)");
#endif
    Serial.println("============================================");

    s_line.reserve(64);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

#if WIFI_MODE == WIFI_MODE_STATIC
    // ---------- 模式1: 写死的账号密码, 直接连接 ----------
    Serial.printf("[WiFi] 使用静态账号: SSID=%s\r\n", WIFI_SSID);
    s_should_save = true;  // 连接成功后也写一份到 Flash (便于以后切换到模式3)
    start_connect(String(WIFI_SSID), String(WIFI_PASSWORD));

#elif WIFI_MODE == WIFI_MODE_SCAN
    // ---------- 模式2: 扫描 + 串口选择 ----------
    Serial.println("[WiFi] 启动扫描, 请通过串口选择 WiFi 并输入密码");
    s_should_save = false;
    start_scan();

#elif WIFI_MODE == WIFI_MODE_SAVED
    // ---------- 模式3: 优先用 Flash 里的, 没有再扫描 ----------
    Serial.println("[WiFi] 检查 Flash 中是否已保存账号...");
    String ssid, pass;
    load_from_flash(ssid, pass);
    if (ssid.length() > 0) {
        Serial.printf("[WiFi] 读取到保存的账号: SSID=%s  PASS=***\r\n", ssid.c_str());
        Serial.println("[WiFi] >>> 直接连接 (无需配网)");
        s_should_save = true;
        start_connect(ssid, pass);
    } else {
        Serial.println("[WiFi] Flash 无保存, 进入扫描配网模式");
        s_should_save = true;  // 第一次配网成功后也要保存
        start_scan();
    }
#endif

    Serial.println("[WiFi] 串口命令: HELP / STATUS / SCAN / RESET");
    Serial.println();
}

void wifi_mgr_update(void)
{
    poll_serial();
    unsigned long now = millis();

    switch (s_state) {
    case WFS_IDLE:
        break;

    case WFS_SCANNING: {
        int n = WiFi.scanComplete();
        if (n > 0) {
            print_scan_result();
            s_state = WFS_SELECT;
        }
        else if (n == 0) {
            print_scan_result();
            Serial.println("[WiFi] 3 秒后自动重扫 (或输入 SCAN 立即重扫)...");
            s_state_enter = now;
            s_state       = WFS_SCAN_WAIT;
        }
        else if (n == -2) {
            Serial.println("[WiFi] 扫描失败, 重试...");
            WiFi.scanDelete();
            start_scan();
        }
        else {
            // n == -1: 扫描进行中
            if (now - s_state_enter > SCAN_TIMEOUT_MS) {
                Serial.println("[WiFi] 扫描超时, 重试...");
                WiFi.scanDelete();
                start_scan();
            }
        }
        break;
    }

    case WFS_SCAN_WAIT:
        if (now - s_state_enter > 3000) {
            WiFi.scanDelete();
            start_scan();
        }
        break;

    case WFS_SELECT:
    case WFS_PASSWORD:
        // 等待串口输入
        break;

    case WFS_CONNECTING:
        if (WiFi.status() == WL_CONNECTED) {
            s_state = WFS_CONNECTED;
            Serial.println();
            Serial.println("[WiFi] ************ 连接成功! ************");
            Serial.printf("[WiFi] SSID        : %s\r\n", WiFi.SSID().c_str());
            Serial.printf("[WiFi] IP 地址     : %s\r\n", WiFi.localIP().toString().c_str());
            Serial.printf("[WiFi] 子网掩码    : %s\r\n", WiFi.subnetMask().toString().c_str());
            Serial.printf("[WiFi] 网关        : %s\r\n", WiFi.gatewayIP().toString().c_str());
            Serial.printf("[WiFi] DNS         : %s\r\n", WiFi.dnsIP().toString().c_str());
            Serial.printf("[WiFi] MAC         : %s\r\n", WiFi.macAddress().c_str());
            Serial.printf("[WiFi] 信号强度    : %d dBm\r\n", WiFi.RSSI());
            Serial.println("[WiFi] **********************************");
            Serial.println();

            if (s_should_save) {
                save_to_flash(s_sel_ssid, s_conn_pass);
            }
        }
        else if (now - s_conn_start > CONNECT_TIMEOUT_MS) {
            Serial.printf("\r\n[WiFi] 连接超时 (%lu ms), 密码可能错误\r\n",
                CONNECT_TIMEOUT_MS);
            WiFi.disconnect();

            if (WIFI_MODE == WIFI_MODE_SAVED) {
                // 高级模式: 保存的账号失效, 清除并进入扫描
                clear_flash();
                Serial.println("[WiFi] 已清除失效的保存账号, 重新进入扫描配网");
            }
            s_state_enter = now;
            s_state       = WFS_SELECT;  // 先回到选择, 等待重新扫描
            // 实际上下面的代码会立刻触发重扫
            WiFi.scanDelete();
            start_scan();
        }
        break;

    case WFS_CONNECTED:
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[WiFi] 检测到连接断开, 尝试自动重连...");
            WiFi.reconnect();
            s_conn_start = now;
            s_state      = WFS_CONNECTING;
        }
        break;

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// 状态查询
// ---------------------------------------------------------------------------
bool wifi_mgr_is_connected(void)
{
    return s_state == WFS_CONNECTED;
}

String wifi_mgr_get_ip(void)
{
    return (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : String("0.0.0.0");
}

String wifi_mgr_get_ssid(void)
{
    return (WiFi.status() == WL_CONNECTED) ? WiFi.SSID() : String("");
}

int wifi_mgr_get_rssi(void)
{
    return (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
}

const char* wifi_mgr_get_state_str(void)
{
    return state_name(s_state);
}

bool wifi_mgr_is_provisioning(void)
{
    return (s_state == WFS_SELECT) || (s_state == WFS_PASSWORD);
}

// ---------------------------------------------------------------------------
// 控制接口
// ---------------------------------------------------------------------------
void wifi_mgr_clear_saved(void)
{
    clear_flash();
    Serial.println("[WiFi] 已清除 Flash 中保存的账号密码 (重启生效)");
}

void wifi_mgr_force_rescan(void)
{
    if (s_state != WFS_CONNECTING) {
        start_scan();
    }
}

void wifi_mgr_reconnect_saved(void)
{
    if (s_state == WFS_CONNECTING) {
        Serial.println("[WiFi] KEY1 ON: 正在连接中, 跳过重连");
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] KEY1 ON: 已连接 SSID=%s, 无需重连\r\n",
                      WiFi.SSID().c_str());
        return;
    }

    Serial.println("[WiFi] KEY1 ON: 重新触发连接...");
    WiFi.disconnect();
    delay(50);

#if WIFI_MODE == WIFI_MODE_SAVED
    String ssid, pass;
    load_from_flash(ssid, pass);
    if (ssid.length() > 0) {
        Serial.printf("[WiFi] KEY1 ON: 使用保存账号 SSID=%s\r\n", ssid.c_str());
        s_should_save = true;
        start_connect(ssid, pass);
    } else {
        Serial.println("[WiFi] KEY1 ON: Flash 无保存, 进入扫描");
        s_should_save = true;
        start_scan();
    }
#elif WIFI_MODE == WIFI_MODE_STATIC
    Serial.printf("[WiFi] KEY1 ON: 使用静态账号 SSID=%s\r\n", WIFI_SSID);
    s_should_save = true;
    start_connect(String(WIFI_SSID), String(WIFI_PASSWORD));
#else
    Serial.println("[WiFi] KEY1 ON: 重新扫描");
    s_should_save = false;
    start_scan();
#endif
}
