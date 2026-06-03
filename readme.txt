================================================================================
  PCT_100_CTL  项目说明
  Version : V4.5   2026-06-02
  Author  : md
================================================================================

一、项目简介
--------------------------------------------------------------------------------
基于 ESP32 的智能控制器 (PCT_100_CTL), 通过自锁开关 + 按键实现:
  - 1 路 LED (光)  继电器控制
  - 1 路 风扇(FAN) 继电器控制
  - 1 路 ADC (光敏电阻, 自动感光)
  - 1 路 DS18B20 温度采集 (TEMP_THRESHOLD=30°C 触发风扇)
  - 0.96 寸 SH1106 OLED 显示 (SDA=4, SCL=5)
  - 1 路 WS2812 RGB 万色灯 (GPIO0): 开机彩虹 / WiFi 状态指示 / 串口测试
  - 2 个按键:
      KEY1 (GPIO20) 自锁开关, 直读物理状态 -> 总闸
      KEY2 (GPIO21) 短按切模式 (手动), 长按 1~2s 切自动/手动

二、按键逻辑
--------------------------------------------------------------------------------
  KEY1 闭合/断开    ->  power_on() / power_off()    (直读, 每次电平变化即生效)
  KEY2 短按 (<1s)  ->  手动模式: 循环切换 1=灯  2=风扇  3=全开  4=全关
  KEY2 长按 1~2s   ->  切换 AUTO / MANUAL
  默认上电: 总闸关 + AUTO 模式, ADC/温度自动控制 LED/风扇

三、WiFi 配网模块  (本版本 V4.1 新增)
--------------------------------------------------------------------------------
新增文件: wifi_mgr.h / wifi_mgr.cpp
集成方式: 在 PCT_100.ino 的 setup() 末尾调用 wifi_mgr_init(),
          loop() 开头调用 wifi_mgr_update().
库依赖  : WiFi.h, Preferences.h  (ESP32 Arduino Core 内置, 无需额外安装)

1) 三种配网模式 (在 wifi_mgr.h 里切换)
   -------------------------------------------------------------------------
   | 层次  | WIFI_MODE 宏           | 行为                                     |
   |-------|------------------------|------------------------------------------|
   | 初步  | WIFI_MODE_STATIC  (1)  | 写死 SSID/PASSWORD (WIFI_SSID/WIFI_PASSWORD), |
   |       |                        | 上电自动连接                              |
   | 升级  | WIFI_MODE_SCAN    (2)  | 异步扫描附近 WiFi -> 串口选序号 -> 输入密码 -> 连接 |
   | 高级  | WIFI_MODE_SAVED   (3)  | 优先用 Flash 保存的账号直接连; 没有则扫描配网; |
   | (默认)|                        | 连接成功后自动写回 Flash; 密码错误自动清除旧账号 |
   -------------------------------------------------------------------------
   切换: 编辑 wifi_mgr.h 第 18 行
        #define WIFI_MODE   WIFI_MODE_SAVED     // 改成 1/2/3 即切换

2) 串口命令 (115200bps, 模式 2/3 配网时使用)
   -------------------------------------------------------------------------
     序号+回车         选择第 N 个扫描到的 WiFi
     密码+回车         输入密码开始连接
     SCAN  / RESCAN    重新扫描附近网络
     STATUS            查看 状态/IP/SSID/RSSI
     DISCONNECT / OFF  断开当前连接 (不清除 Flash 保存)
     RECONNECT / RC    重新连接 (模式1用宏, 模式3读Flash, 模式2重扫)
     RESET  / CLEAR    清除 Flash 中保存的账号 (要换 WiFi 时用)
     HELP   / ?        命令列表
   -------------------------------------------------------------------------

3) 三种模式详细流程

   【模式 1 STATIC - 初步要求】
     上电 -> 直接 WiFi.begin(WIFI_SSID, WIFI_PASSWORD)
           -> 串口打印连接进度, 成功后打印 IP/SSID/RSSI/MAC 等
           -> 同时把账号密码写一份到 Flash (切换到模式3后可直接用)

   【模式 2 SCAN - 升级要求】
     上电 -> WiFi.scanNetworks(true) 异步扫描
           -> 串口打印网络列表 (序号/信号/加密/SSID)
           -> 等待用户输入序号, 选完后提示输入密码
           -> 调用 WiFi.begin(), 等待 WL_CONNECTED
           -> 成功后打印 IP 等信息 (不写 Flash)

   【模式 3 SAVED - 高级要求, 默认】
     首次上电:  Flash 空 -> 进入扫描 -> 选序号 + 密码 -> 连上后自动保存
     再次上电:  Flash 有 -> 直接连接, 串口无任何提示
     密码错误:  20s 超时 -> 自动清除旧账号 -> 重新进入扫描配网
     换 WiFi:   串口发 RESET 清空 -> 重启 -> 重新配网

4) Flash 存储说明
   -------------------------------------------------------------------------
   库   : Preferences.h (ESP32 NVS 封装)
   命名空间: "pct100"
   键名  : "ssid"  -> WiFi SSID
           "pass"  -> WiFi 密码
   寿命  : 写次数约 10 万次, 断电不丢失
   擦除  : 串口发 RESET / CLEAR, 或调用 wifi_mgr_clear_saved()

5) 串口输出示例 (模式 3 首次配网)
   -------------------------------------------------------------------------
   ============================================
             WiFi 配网模块 初始化
             模式: 保存自动连接 (3)
   ============================================
   [WiFi] 检查 Flash 中是否已保存账号...
   [WiFi] Flash 无保存, 进入扫描配网模式
   [WiFi] 正在扫描附近网络...
   [WiFi] 扫描完成, 共发现 5 个网络:
       序号  信号       加密类型    SSID
       ----  ---------  ----------  ----------------
         0    -45 dBm  WPA2-PSK    TP-LINK_HOME
         1    -67 dBm  WPA2-PSK    ChinaNet-XXXX
         2    -78 dBm  开放        FREE_WIFI
         3    -82 dBm  WPA3-PSK    MyRouter5G
         4    -89 dBm  WPA2-PSK    Neighbor
   [WiFi] 请输入要连接的序号 (0 ~ 4): 0
   [WiFi] 已选择: [0] "TP-LINK_HOME"  信号=-45 dBm  加密=WPA2-PSK
   [WiFi] 请输入密码: 12345678
   [WiFi] >>> 正在连接 SSID="TP-LINK_HOME" ...
   [WiFi] ************ 连接成功! ************
   [WiFi] SSID        : TP-LINK_HOME
   [WiFi] IP 地址     : 192.168.1.105
   [WiFi] 子网掩码    : 255.255.255.0
   [WiFi] 网关        : 192.168.1.1
   [WiFi] DNS         : 192.168.1.1
   [WiFi] MAC         : AA:BB:CC:DD:EE:FF
   [WiFi] 信号强度    : -45 dBm
   [WiFi] **********************************
   [WiFi] >>> 已保存到 Flash: SSID=TP-LINK_HOME (下次上电自动连接)

6) 公开 API (可在其它模块调用)
   -------------------------------------------------------------------------
   bool    wifi_mgr_is_connected(void);   // 是否已连接
   String  wifi_mgr_get_ip(void);          // IP, 未连接返回 "0.0.0.0"
   String  wifi_mgr_get_ssid(void);        // 已连接返回 SSID
   int     wifi_mgr_get_rssi(void);        // 信号强度 dBm
   const char* wifi_mgr_get_state_str(void); // IDLE/SCANNING/SELECT/...
   bool    wifi_mgr_is_provisioning(void); // 是否在等待用户输入
   void    wifi_mgr_clear_saved(void);     // 清除保存的账号
   void    wifi_mgr_force_rescan(void);    // 强制重新扫描

================================================================================
版本历史
--------------------------------------------------------------------------------
V1.0  20260527  创建项目初始文件: 按键及继电器控制
V1.2            添加 .gitignore
V2.0            重构: 改用轮询消抖, 修复引脚冲突与看门狗复位
V2.1            自锁开关 KEY1 直读状态, KEY2 长按切换自动/手动
V2.2            开机全灭, 长按改为 2s
V2.3            开机防噪声误判, 自动模式全...
V3.4            KEY1 自锁开关直读物理状态
V3.5            新增 ADC 电压检测自动模式功能
V4.0            重构按键模块, 修复手动模式误切自动模式问题
V4.1   20260602  新增 WiFi 配网模块 (wifi_mgr.h / wifi_mgr.cpp):
                  1. 模式 1 (WIFI_MODE_STATIC)   写死账号密码上电直连
                  2. 模式 2 (WIFI_MODE_SCAN)     串口扫描+选择+输入密码
                  3. 模式 3 (WIFI_MODE_SAVED)    Flash 保存+自动重连 (默认)
                  串口命令: SCAN/STATUS/DISCONNECT/RECONNECT/RESET/HELP
                  DISCONNECT 断开当前连接, RECONNECT 重新连接
                  Flash 用 Preferences.h 存于 NVS 命名空间 "pct100"
V4.2   20260602  新增 WS2812 RGB 万色灯模块 (rgb_led.h / rgb_led.cpp):
                  - 开机 4 秒 7 色循环闪烁提示
                  - WiFi 已连: HSV 色相平滑呼吸 (5 秒/轮)
                  - WiFi 未连: 红色慢呼吸 (3 秒/轮, 5%~30% 亮度)
                  - 串口命令: RGB R G B / RGB #RRGGBB / LED OFF
                  - GPIO0 启动前 delay(100) 避开 strapping 窗口
V4.3   20260602  RGB 灯状态机调整:
                  - 开机提示: 7 色循环 (4s) -> 彩色快速闪烁 (2.1s, 100ms/色)
                  - 新增 BOOT_SUCCESS: KEY1 开启后 1.5s HSV 高速彩虹庆祝
                  - WiFi 已连: HSV 7 色呼吸 (5s) -> 纯绿色呼吸 (3s)
                  - WiFi 未连: 红色慢呼吸 (不变)
V4.4   20260602  WiFi 配网 KEY1 ON 触发重连:
                  - 新增 wifi_mgr_reconnect_saved(): 读 Flash 并启动连接
                  - power_on() 中调用, KEY1 OFF->ON 时若未连则重连
                  - 幂等: 正在连接/已连接 时跳过, 不重复触发
                  - 三种模式均支持 (静态/扫描/保存)
V4.5   20260602  RGB 灯状态同步修复:
                  - 新增 rgb_led_get_mode() 返回 cur_mode 权威源
                  - PCT_100.ino loop() 改用 get_mode 替代 last_rgb 缓存
                  - 修复 BOOT_SUCCESS 结束后 LED 卡在 WIFI_DISC 不切换的 bug
================================================================================
