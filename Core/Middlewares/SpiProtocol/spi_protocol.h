/**
 * ===========================================================================
 * SPI 协议层 — MOSI 打包 / MISO 解析 / RX 回调处理
 * ===========================================================================
 */
#pragma once
#include "com_def.h"
#include "scheduler_data.h"
#include "dispatcher_cmd.h"
#include "bsp_spi.h"

/* 注册为 BSP SPI 驱动的 RX 回调，连接协议解析与业务模块 */
void SpiProto_RegisterAsRxCallback(void);

/* 将 UpFrame 打包为 1024B MOSI wire buffer */
void SpiProto_PackUpFrame(const UpFrame* f, uint8_t wire[SPI_WIRE_SIZE]);

/* 设置当前发送中的模块（DMA 完成后自动 flush） */
void SpiProto_SetActiveOwner(DataModule_t* owner);
