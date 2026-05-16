/**
 * ===========================================================================
 * 测试上行模块实现
 *
 * 帧格式（ESP32 端观察）：
 *   MOSI[0]=0x01  [1]=0xEE  [2..3]=0x0010  [4..19]=同步字AA55CC33 + 计数器
 * ===========================================================================
 */
#include "module_test.h"
#include "cmsis_os2.h"
#include <string.h>

static UpFrame DMA_ALIGNED4 s_frame;
static volatile bool s_ready = false;
static osSemaphoreId_t s_sem;

/* ====================== DataModule 接口实现 ====================== */
static void Init(void)
{
    s_sem = osSemaphoreNew(1, 1, NULL);
    s_ready = false;
    memset(&s_frame, 0, sizeof(s_frame));
    s_frame.data_type = 0xEE;
}

static void Collect(void)
{
    if (osSemaphoreAcquire(s_sem, 0) != osOK) return;

    static uint32_t counter = 0;
    counter++;

    s_frame.data_len = 16;
    s_frame.data_buf[0] = 0xAA;
    s_frame.data_buf[1] = 0x55;
    s_frame.data_buf[2] = 0xCC;
    s_frame.data_buf[3] = 0x33;
    s_frame.data_buf[4] = (uint8_t)(counter & 0xFF);
    s_frame.data_buf[5] = (uint8_t)((counter >> 8) & 0xFF);
    s_frame.data_buf[6] = (uint8_t)((counter >> 16) & 0xFF);
    s_frame.data_buf[7] = (uint8_t)((counter >> 24) & 0xFF);

    s_ready = true;
}

static bool     IsReady(void)      { return s_ready; }
static UpFrame* GetFrame(void)     { return &s_frame; }
static uint8_t  GetPrio(void)      { return 10; }

static void Flush(void)
{
    s_ready = false;
    osSemaphoreRelease(s_sem);
}

DataModule_t g_data_module_test = {
    .init       = Init,
    .collect    = Collect,
    .is_ready   = IsReady,
    .get_frame  = GetFrame,
    .get_prio   = GetPrio,
    .flush      = Flush,
};

/* ====================== 采集任务 ====================== */
void Test_Task(void* argument)
{
    (void)argument;
    while (1) {
        g_data_module_test.collect();
        osDelay(100);
    }
}
