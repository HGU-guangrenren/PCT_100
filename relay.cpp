#include "relay.h"

/**
 * @brief 初始化继电器引脚
 * 将继电器控制引脚配置为输出模式，并设置初始状态为关闭
 */
void relay_init(void)
{
    pinMode(RELAY_PIN, OUTPUT);
    relay_off();  // 初始状态：继电器关闭
}

/**
 * @brief 打开继电器
 * 输出高电平，触发继电器吸合（COM与NO导通）
 */
void relay_on(void)
{
    digitalWrite(RELAY_PIN, RELAY_ON);
}

/**
 * @brief 关闭继电器
 * 输出低电平，继电器释放（COM与NC导通）
 */
void relay_off(void)
{
    digitalWrite(RELAY_PIN, RELAY_OFF);
}

/**
 * @brief 翻转继电器状态
 * 如果当前是打开状态则关闭，反之亦然
 */
void relay_toggle(void)
{
    digitalWrite(RELAY_PIN, !digitalRead(RELAY_PIN));
}