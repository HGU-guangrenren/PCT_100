#include "relay.h"

/**
 * @brief 初始化函数
 * 只运行一次，用于初始化硬件
 */
void setup()
{
    relay_init();  // 初始化继电器
}

/**
 * @brief 主循环函数
 * 无限循环执行，实现继电器周期性开关
 */
void loop()
{
    relay_on();   // 打开继电器（负载工作）
    delay(1000);  // 保持1秒
    
    relay_off();  // 关闭继电器（负载停止）
    delay(1000);  // 保持1秒
}