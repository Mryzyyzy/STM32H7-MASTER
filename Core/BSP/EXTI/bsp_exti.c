/**
 * ===========================================================================
 * BSP 驱动层 — EXTI 中断实现（ESP32 IRQ: PA0）
 * ===========================================================================
 */
#include "bsp_exti.h"

/* ====================== 板级定义 ====================== */
#define IRQ_PORT    GPIOA
#define IRQ_PIN     GPIO_PIN_0

/* ====================== 中断标志 ====================== */
static volatile bool s_irq_flag = false;

/* ====================== 初始化 ====================== */
void BspExti_Init(void)
{
    s_irq_flag = false;
}

/* ====================== 状态查询 ====================== */
bool BspExti_IsRequested(void)
{
    return s_irq_flag || (HAL_GPIO_ReadPin(IRQ_PORT, IRQ_PIN) == GPIO_PIN_SET);
}

void BspExti_ClearRequest(void) { s_irq_flag = false; }

bool BspExti_ReadPin(void)
{
    return (HAL_GPIO_ReadPin(IRQ_PORT, IRQ_PIN) == GPIO_PIN_SET);
}

/* ====================== EXTI 中断回调 (weak override) ====================== */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == IRQ_PIN) {
        s_irq_flag = true;
    }
}
