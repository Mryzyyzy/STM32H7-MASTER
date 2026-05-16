/**
 * ===========================================================================
 * BSP 驱动层 — EXTI 外部中断（ESP32 IRQ 引脚）
 * ===========================================================================
 *
 * PA0 = ESP32 IRQ (电平触发，HIGH=就绪 LOW=等待)
 * EXTI 中断仅置位软件标志，主循环轮询引脚电平防止漏信号。
 */
#pragma once
#include "stm32h7xx_hal.h"
#include <stdbool.h>

/* ====================== API ====================== */
void BspExti_Init(void);
bool BspExti_IsRequested(void);          /* 是否有 IRQ 请求（EXTI 标志 OR 引脚高电平） */
void BspExti_ClearRequest(void);         /* 消费 IRQ 标志 */
bool BspExti_ReadPin(void);              /* 读取 IRQ 引脚当前电平 */
