/**
 * ===========================================================================
 * BSP 驱动层 — SPI 全双工 DMA 驱动
 * ===========================================================================
 *
 * 适配硬件：STM32H743 SPI2
 *   MOSI=PC1  MISO=PC2  SCK=PB13  CS=PC3
 *   DMA1_Stream0(RX)  DMA1_Stream1(TX)
 *
 * DMA 缓冲区放在 AXI SRAM (0x24000000, section .dma_bss)，
 * 因为 STM32H7 DMA1 无法访问 DTCMRAM。
 */
#pragma once
#include "stm32h7xx_hal.h"
#include "com_def.h"

/* ====================== 板级 SPI 句柄 ====================== */
extern SPI_HandleTypeDef hspi2;
#define BSP_SPI_HANDLE  (&hspi2)

/* ====================== MISO 接收回调类型 ====================== */
typedef void (*BspSpi_RxCallback)(const uint8_t* rx, uint16_t len, SpiWorkState state);

/* ====================== API ====================== */
void     BspSpi_Init(SPI_HandleTypeDef* hspi);
void     BspSpi_RegisterRxCb(BspSpi_RxCallback cb);
HAL_StatusTypeDef BspSpi_StartFullDuplex(uint8_t* tx_ptr, uint16_t len);
HAL_StatusTypeDef BspSpi_StartPoll(void);
