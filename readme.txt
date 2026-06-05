================================================================================
  PCT_100_CTL  项目说明
  Version : V6.0.1  2026-06-03
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
四、MQTT 通信模块  (V4.7~V4.8 新增)
--------------------------------------------------------------------------------
新增文件: mqtt_mgr.h / mqtt_mgr.cpp / console.h / console.cpp
库依赖  : PubSubClient.h, ArduinoJson.h (Arduino 库管理器安装)

1) 通信协议 (固定, 来自上位机)
   -------------------------------------------------------------------------
    Broker       : your_broker_ip:8081  (TCP, 非 SSL)
    协议版本     : MQTT 3.1.1
    用户名 / 密码: your_username / your_password
   设备 ID      : 运行期变量, 默认 PCT_100_005 (version.h 中 DEVICE_ID_DEFAULT)
   上行 topic   : chemctrl/{id}/status   (10 字段 JSON)
   下行 topic   : chemctrl/{id}/command  (5 种 cmd)
   LWT topic    : chemctrl/{id}/lwt      (retain, online true/false)

2) 上行 status 字段 (10 项)
   -------------------------------------------------------------------------
   temperature      float   温度 (°C)
   light            int     光照 (lux)
   mode             str     "auto" / "manual"
   key1_lock        bool    KEY1 物理状态
   relay3           bool    灯光继电器 (GPIO6)
   relay4           bool    风扇继电器 (GPIO7)
   temp_threshold   float   温度高值阈值 (默认 30.0)
   light_threshold  int     光照低值阈值 (默认 300)
   -------------------------------------------------------------------------
   触发上报: 60s 慢心跳兜底 / 收到 get_status / 命令处理完回包

3) 下行 5 种 cmd
   -------------------------------------------------------------------------
   set_relay        relay: 3-4, value: bool
                    控制继电器; KEY1 OFF 时静默忽略
                    relay 3 -> 灯光, relay 4 -> 风扇
   set_mode         mode: "auto" / "manual"
                    切换自动/手动模式
   get_status       (无参)
                    设备立即上报一次完整 status
   set_threshold    temp: float, light: int (可独立传)
                    设置温度高值 / 光照低值阈值, NVS 持久化
   reboot           (无参)
                    设备先 publish 一次 status, 1s 后 ESP.restart()

4) 串口命令 (设备端配置 MQTT 连接, V4.8 新增)
   -------------------------------------------------------------------------
   MQTT                     帮助 (打印命令列表)
   MQTT SHOW                显示 5 项配置 + 连接状态
    MQTT SET IP <ip>         例: MQTT SET IP your_broker_ip
    MQTT SET PORT <port>     例: MQTT SET PORT 8081
    MQTT SET USER <user>     例: MQTT SET USER your_username
    MQTT SET PASS <pass>     例: MQTT SET PASS your_password
    MQTT SET ID <id>         例: MQTT SET ID PCT_100_005
    MQTT SET LIGHT_TH <lux>  例: MQTT SET LIGHT_TH 150   (V5.3)
    MQTT SET TEMP_TH  <°C>   例: MQTT SET TEMP_TH 30.0   (V5.3)
    MQTT CLEAR               恢复 5 项出厂默认 + 写 NVS + 自动重连
    MQTT RECONNECT           强制 disconnect, 下次 update 自动重连
    -------------------------------------------------------------------------
    每条 SET 立即生效: 写变量 + 写 NVS + 自动 disconnect
                      下次 loop 的 mqtt_mgr_update() 用新值重连
    LIGHT_TH / TEMP_TH 的串口效果等同于 MQTT 远程 set_threshold 命令

5) NVS 存储 (Preferences.h, 命名空间 pct100)
   -------------------------------------------------------------------------
   键名: mqtt_ip / mqtt_port / mqtt_user / mqtt_pass / mqtt_id
   缺失时用 mqtt_mgr.h 顶部 MQTT_*_DEFAULT 默认值

6) MQTTX 使用 (PC 端工具, 免费, 跨平台 Win/Mac/Linux)
   -------------------------------------------------------------------------
   MQTTX 是 EMQX 公司出品的桌面 MQTT 客户端, 用来在 PC 上
   观察板子上报的消息 / 主动下发命令 / 调试多设备, 排查 "上位机
   显示设备未连接" 时也用它验证 broker 是否可达.

   【1. 下载安装】
      官网    : https://mqttx.app/zh
      GitHub  : https://github.com/emqx/MQTTX/releases
      选 mqttx-x.x.x-windows-x64.exe (Win) / .dmg (Mac) / .AppImage (Linux)

   【2. 新建连接 (Create New Connection)】
      顶部 [+] -> Connections -> 填:
        Name        : 任意标识 (如 debug_PCT100_005)
        Host        : your_broker_ip
        Port        : 8081
        Client ID   : 必须与设备不同, 建议 mqttx_<你名>_<序号>
                       如 mqttx_lyg_001 (用设备 ID 会被 broker 踢)
        Username    : your_username
        Password    : your_password
        MQTT Version: 3.1.1
        SSL/TLS     : 关闭
        Keep Alive  : 60
        Auto Reconnect: 勾选
        Clean Session: 勾选 (默认就是, 不用改)
      点右上 [Connect] -> 左侧连接变绿 = 连上 broker

   【3. 订阅 topic (看板子上报)】
      顶部 [+] -> New Subscription, 逐条加:
        Topic : chemctrl/PCT_100_005/status   QoS 1   Retain 不勾
        Topic : chemctrl/PCT_100_005/lwt      QoS 1   Retain 勾上
        Topic : chemctrl/+/status             QoS 1   Retain 不勾
                (通配订阅所有设备, 用 + 匹配 device_id 一段)
        Topic : chemctrl/+/lwt                QoS 1   Retain 勾上
      点 [Subscribe] 后下方消息区会持续滚出设备主动 publish 的 JSON.
      60 秒没新消息属于正常 (慢心跳兜底周期), 想立即看可手动发
      get_status 命令 (见下).

   【4. 看懂板子 status 消息 (10 字段 JSON)】
      收到消息示例:
        Topic  : chemctrl/PCT_100_005/status
        Payload: {"temperature":25.3,"light":680,"mode":"auto",
                  "key1_lock":true,"relay3":false,"relay4":true,
                  "temp_threshold":40.0,"light_threshold":320}
      字段含义同上方 "2) 上行 status 字段".

   【5. 主动下发命令到板子 (模拟上位机)】
      底部输入框:
        Topic   : chemctrl/PCT_100_005/command
        QoS     : 1
        Retain  : 不勾
      Payload 填 JSON (大括号, 双引号, 无 BOM):
        {"cmd":"get_status"}                       立即拉一次完整 status
        {"cmd":"set_relay","relay":3,"value":true}  灯亮
        {"cmd":"set_relay","relay":4,"value":true}  风扇转
        {"cmd":"set_relay","relay":3,"value":false} 灯灭
        {"cmd":"set_mode","mode":"manual"}          切手动
        {"cmd":"set_mode","mode":"auto"}            切自动
        {"cmd":"set_threshold","temp":35.0,"light":250} 改阈值 (可只传一个)
        {"cmd":"reboot"}                            远程重启
      点右下发送 -> 串口应有对应打印 -> 几秒内 status 也会更新.

   【6. 排查 "上位机显示设备未连接" 步骤】
      (a) MQTTX 能否连上 broker?   连不上 -> PC 网络/防火墙问题
      (b) 订阅 chemctrl/+/status  能否收到板子消息?
          收不到 -> 板子端没连 broker (看 OLED 第四行 "云端:已连?")
          收到   -> broker+板子都正常, 问题在上位机侧 (device_id 不匹配等)
      (c) 主动 publish get_status 板子有无回包?  有 -> 双向通, 排查上位机.

   【7. 多设备同时监控】
      同一台 PC 可建多个连接, 每个连接用不同 Client ID:
        debug_dev01 -> 订阅 chemctrl/PCT_100_001/+
        debug_dev02 -> 订阅 chemctrl/PCT_100_002/+
      或都用 Client ID A, 订阅 chemctrl/+/+ 一次看全.
      调试完记得点 [Disconnect] 释放 broker 连接 (避免挤占设备资源).

   【8. 常见问题】
      Q: MQTTX 连不上 broker?
      A: 1) 防火墙是否放行 8081 TCP 出站
          2) PC 是否能 ping 通 your_broker_ip
         3) Keep Alive 不填也行, 默认 60
      Q: 订阅了但收不到 status?
      A: 1) QoS 必须选 1 (板子上行用 qos1)
         2) Topic 拼写错 (下划线 vs 斜杠, 区分大小写)
         3) 看板子串口 [MQTT] 连接成功 打过没
      Q: 发了命令板子没反应?
      A: 1) JSON 格式错 (花括号, 逗号, 字符串双引号)
         2) Topic 写到 chemctrl/PCT_100_005/status (反了, 应是 /command)
         3) KEY1 OFF 时 set_relay 被静默忽略, 不算 bug

7) ESP32-C3 烧录 + 实操流程
   -------------------------------------------------------------------------
   Arduino IDE:
     开发板: ESP32C3 Dev Module
     Flash : 4MB
     Partition Scheme: Default 4MB with spiffs (NVS 需要 spiffs 分区)
     烧录时按住 BOOT 按钮直到上传开始
   串口 115200 看启动日志, 应有:
     [MQTT] NVS 加载: your_broker_ip:8081 user=your_username id=PCT_100_005
     [MQTT] 连接成功
   测试流程:
     (1) MQTTX 发 {"cmd":"get_status"} -> 立即收到 status
     (2) 发 {"cmd":"set_relay","relay":3,"value":true} -> 灯亮 (继电器咔哒)
     (3) 发 {"cmd":"set_relay","relay":4,"value":true} -> 风扇转
     (4) 发 {"cmd":"set_threshold","temp":35.0,"light":250} -> 阈值更新
     (5) 拔电源重插, OLED 显示 35.0/250 -> NVS 持久化验证
     (6) KEY1 OFF -> set_relay 静默忽略, 串口打印 [MQTT] set_relay 拒绝: KEY1 OFF

8) 常见问题
   -------------------------------------------------------------------------
   Q: 设备 rc= 错误?
   A: rc=-2 凭证错或 Client ID 冲突; rc=-4 网络超时
      试 MQTT RECONNECT 强制重连, 或 MQTT SET USER/PASS 重新触发
   Q: MQTTX 收不到上报?
   A: 1) 确认订阅了正确的 status topic
      2) 串口看设备 [MQTT] 连接成功?
      3) 主动发 get_status 触发上报
   Q: ESP32-C3 烧录失败?
   A: 1) USB 数据线 (非只充电线)
      2) 烧录时按住 BOOT 按钮
      3) Arduino IDE 开发板选 ESP32C3 Dev Module
   Q: OLED 无显示?
   A: 1) SDA=4, SCL=5 接对
      2) 串口看 [OLED] 阈值已加载 (init 成功)
      3) I2C 地址 (默认 0x3C)

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
V4.6   20260602  RGB 灯 WiFi 状态读取修复 (KEY1 OFF->ON 卡红):
                  - wifi_mgr_is_connected() 改为读 s_state 而非 WiFi.status()
                  - 修复 KEY1 ON 后 ESP32 WiFi 栈短暂抖动导致 LED 卡红色 bug
                  - 重启流程不受影响 (仍能正确变绿)
V4.7   20260602  新增 MQTT 通信模块 (mqtt_mgr.h / mqtt_mgr.cpp):
                   - Broker: your_broker_ip:8081, MQTT 3.1.1, your_username/your_password
                  - 设备 ID 用 version.h DEVICE_ID 宏 (烧录前改为丝印序列号)
                  - 上行 topic: chemctrl/{id}/status, 10 字段 JSON
                  - 下行 topic: chemctrl/{id}/command, 支持 5 种 cmd:
                    set_relay (受 KEY1 约束) / get_status / set_mode /
                    set_threshold / reboot
                  - LWT 遗嘱: chemctrl/{id}/lwt, retain+qos1
                  - 60s 慢心跳兜底 + server 主动 get_status 拉取并存
                  - 阈值改为变量 (g_temp_threshold / g_light_threshold)
                    NVS 持久化, MQTT 远程可改
                  - OLED 加 5 行布局: 模式/光照/温度/WiFi+云端/灯+风扇
                    新增 WiFi 与云端连接状态显示
V4.8   20260602  MQTT 配置串口可改 + 持久化:
                  - mqtt_mgr.h: 硬编码宏 -> 运行期变量 + 默认值宏
                  - mqtt_mgr.cpp: NVS 加载/保存 (命名空间 pct100)
                    5 键: mqtt_ip / mqtt_port / mqtt_user / mqtt_pass / mqtt_id
                  - 串口命令 8 条 (一次性, 立即生效 + 写 NVS + 自动重连):
                    MQTT SHOW
                    MQTT SET IP <ip> / PORT <port> / USER <u> / PASS <p> / ID <id>
                    MQTT CLEAR      恢复默认
                    MQTT RECONNECT  强制重连
                  - DEVICE_ID 改为运行期变量, version.h 仅保留默认值宏
                  - 新增 console.h / console.cpp 串口行级分发器
                    解决多 module 共享 Serial 输入的竞争
                  - rgb_led_console 改用 console_take/give_back
V4.9   20260602  readme.txt 补充 MQTT 通信模块完整使用说明:
                  - 新增章节 "四、MQTT 通信模块" 包含:
                    1) 通信协议 (broker/端口/凭证/device_id/topic)
                    2) 上行 status 10 字段说明
                    3) 下行 5 种 cmd payload 模板
                    4) 8 条设备端串口 MQTT 命令
                    5) NVS 存储键名
                    6) MQTTX (PC 端) 完整连接步骤
                    7) ESP32-C3 烧录 + 实操流程
                    8) 常见问题排查
V5.0   20260603  修复 MQTT 连接失败时日志刷屏 (加重试间隔退避):
                   - mqtt_mgr.h: 加 MQTT_RETRY_INTERVAL_MS = 5000UL
                   - try_connect() 成功/失败均记录时间戳
                   - mqtt_mgr_update() 在 !connected 分支按 5s 间隔重试
                   - 端口 RST 场景 (PORT=9999): 每帧 50ms 刷屏 -> 每 5s 一次
                   - 7 个 setter/clear/reconnect 调 reset_retry_timer()
V5.1   20260603  wifi_mgr 接入 console 体系, 修复串口命令被 WiFi 抢:
                   - 根因: V4.8 加 console 体系时漏改 wifi_mgr, poll_serial
                     抢光 Serial 缓冲, mqtt_mgr_console 收不到用户命令
                   - 删 poll_serial(), 新增 wifi_mgr_console() 用 console_take
                   - loop() 顺序: console_pump + 3 个 console 放最前
                   - 删无线残留的 s_line 静态变量
V5.1.1 20260603  修复 KEY2 长按 >2 秒不切模式 (LONG_PRESS_MAX 太严):
                   - 长按检测: 区间 [MIN, MAX] -> 一次性触发 (long_press_triggered)
                   - 按住达到 1s 立即触发, 不限上限, 松手重置
                   - LONG_PRESS_MAX 宏保留 (加注释说明已不使用)
V5.2   20260603  readme.txt 扩充 MQTTX 使用说明为 8 小节:
                  1) 下载安装 (官网 + GitHub releases)
                  2) 新建连接 (10 项参数逐项说明)
                  3) 订阅 topic (4 条常用订阅, 含 + 通配)
                  4) 看懂 status 消息 (JSON 字段示例)
                  5) 主动下发命令 (5 种 cmd payload 模板, 模拟上位机)
                  6) 排查 "上位机显示设备未连接" 步骤
                  7) 多设备同时监控
                  8) 常见问题
V5.3   20260603  修复灯不按阈值开关 (核心 bug):
                  自动模式灯控制从 oled_get_voltage() > 2.0f 写死
                  改为 lux < g_light_threshold, 真正用阈值控制
                  MQTT SET LIGHT_TH/TEMP_TH 串口命令
                  g_light_threshold 默认值 300 -> 150 (新用户默认)
                                     readme 补充 LIGHT_TH/TEMP_TH 命令说明
V6.0   20260603  RGB 指示灯 6 种状态算法:
                   新增 OFF / ALARM_SEVERE / ALARM_LIGHT / ALARM_TEMP
                   / MQTT_DISC / IDLE 六种枚举
                   优先级: 严重(灯+扇) > 光照 > 温度 > WiFi > MQTT > 正常
                   #1 红红绿绿蓝蓝 1200ms / #2 红→绿→蓝 1500ms
                   #3 红→绿→蓝→灭 2200ms / #5 红→绿→灭 900ms
                   #4 MQTT未连 HSV渐变 2000ms / #6 全部正常 HSV渐变 4200ms
V6.0.1 20260603  修复所有报警卡红色 bug:
                    根因: rgb_led.cpp 遗留 anim_ms = now; 每帧重置动画时间
                    所有新模式用 (now-anim_ms)%周期 计算位置, 结果恒 0
                    全部卡在第一个颜色 (红色), 删该行即修复
V6.2   20260603  回退 FreeRTOS 任务分离 + 1s TCP 短超时:
                    回退 V6.1 全部改动 (FreeRTOS 任务/mutex/锁包裹)
                    替代: try_connect() 中用 WiFiClient.setTimeout(1000)
                    将 TCP connect 超时从默认 5s 缩短为 1s
                    连不上时 loop 只卡 1s 而非 5s, 按键/灯/传感器不卡顿
                       version.h: PROJECT_DATE 更新为 2026-06-03
V6.3   20260603  MQTTX 灯/扇状态实时刷新加速:
                      MQTT_HEARTBEAT_MS 60s→5s (兜底刷新上限)
                      loop 末尾加 mqtt_mgr_publish_status()
                      物理按键/自动模式切灯扇后即时上报, MQTTX 秒见
                      配套: V6.3, readme 版本历史 + 注释更新
================================================================================
