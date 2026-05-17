/**
 * ===========================================================================
 * 应用调度层 — SPI_Main_Task 实现（10ms 周期）
 * ===========================================================================
 *
 * 调度策略：
 *   1. 读 IRQ 电平 + EXTI 标志 → ESP32 是否就绪
 *   2. IRQ=HIGH 且 SPI 空闲 → 取最高优先级上行帧发送
 *   3. 无上行帧 → 发 poll 空帧查询 ESP32 是否有待发命令
 *   4. IRQ=LOW → 等待（ESP32 缓冲耗尽）
 *
 * MISO 命令可在任意返回帧中 piggyback，DMA 回调自动解析分发。
 */
#include "spi_master.h"
#include "com_def.h"
#include "spi_protocol.h"
#include "scheduler_data.h"
#include "bsp_spi.h"
#include "bsp_exti.h"
#include "cmsis_os2.h"

/* ====================== 应用初始化 ====================== */
static void AppInit(void)
{
    /* 初始化所有上行模块 */
    for (uint32_t i = 0; i < g_data_module_num; i++) {
        if (g_data_module_list[i] && g_data_module_list[i]->init)
            g_data_module_list[i]->init();
    }

    /* 初始化所有下行指令模块 */
    for (uint32_t i = 0; i < g_cmd_module_num; i++) {
        if (g_cmd_module_list[i] && g_cmd_module_list[i]->init)
            g_cmd_module_list[i]->init();
    }

    /* BSP 驱动初始化 */
    BspExti_Init();
    BspSpi_Init(BSP_SPI_HANDLE);
    SpiProto_RegisterAsRxCallback();
}

/* ====================== SPI 主任务 ====================== */
void SPI_Main_Task(void* argument)
{
    (void)argument;
    AppInit();
    int cnt = 0;
    __attribute__((section(".dma_bss"), aligned(4)))
    static uint8_t wire_tx[SPI_WIRE_SIZE];

    while (1) {
        if (!g_spi_busy) {
            if (BspExti_IsRequested()) {
                BspExti_ClearRequest();

                DataModule_t* owner = NULL;
                UpFrame* f = DataScheduler_GetHighestPrioFrame(&owner);

                if (f) {
                    g_spi_state = SPI_TX_NORMAL;
                    uint16_t xfer_len = SpiProto_PackUpFrame(f, wire_tx);
                    SpiProto_SetActiveOwner(owner);

                    if (BspSpi_StartFullDuplex(wire_tx, xfer_len) != HAL_OK) {
                        SpiProto_SetActiveOwner(NULL);
                        if (owner && owner->flush) owner->flush();
                    }
                } else {
                    if(cnt++ == 100){
                        (void)BspSpi_StartPoll();
                        cnt = 0;
                    }
                }
            }
        }
        osDelay(10);
    }
}
