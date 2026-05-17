/**
 * ===========================================================================
 * 摄像头采集模块（JPEG 模式，SPI 上行单帧发送）
 * ===========================================================================
 *
 * 帧格式（ESP32 端观察）：
 *   MOSI[0]=0x01  [1]=DATA_TYPE_IMG_FRAG  [2..3]=jpg_len(BE)  [4..]=JPEG 数据
 * ===========================================================================
 */
#include "cam_module.h"
#include "bsp_cam.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>

// /* ---- RGB565 连续模式，LCD 显示 ---- */
// void Camera_Task(void *argument)
// {
//     (void)argument;

//     SPI_LCD_Init();
//     LCD_DisplayString(10, 130, "Waiting...");

//     if (DCMI_OV5640_Init() != OV5640_Success) {
//         LCD_DisplayString(10, 130, "Cam Error!");
//         while (1) osDelay(1000);
//     }

//     OV5640_DMA_Transmit_Continuous(CAM_Buffer, CAM_DMA_BufferSize);

//     while (1) {
//         if (OV5640_FrameState == 1) {
//             OV5640_FrameState = 0;

//             LCD_CopyBuffer(0, 0, CAM_DISPLAY_Width, CAM_DISPLAY_Height,
//                            (uint16_t *)(uintptr_t)CAM_Buffer_ADDR);

//             char buf[16];
//             sprintf(buf, "fps:%d", OV5640_FPS);
//             LCD_DisplayString(34, 252, buf);
//         }
//         osDelay(1);
//     }
// }

__attribute__((section(".dma_bss"), aligned(4)))
static struct {
    bool     initialized;
    uint32_t frame_id;
    bool     ready;
    osSemaphoreId_t sem;
    UpFrame  frame;
} s_cam;

/* ====================== DataModule 接口实现 ====================== */
static void Init(void)
{
    s_cam.sem = osSemaphoreNew(1, 1, NULL);
    s_cam.ready       = false;
    s_cam.initialized = false;
    s_cam.frame_id    = 0;
    memset(&s_cam.frame, 0, sizeof(s_cam.frame));
}

static void Collect(void)
{
    if (osSemaphoreAcquire(s_cam.sem, 0) != osOK) return;

    /* 首次初始化 JPEG 模式 */
    if (!s_cam.initialized) {
        if (DCMI_OV5640_JPEG_Init() != OV5640_Success) {
            osSemaphoreRelease(s_cam.sem);
            return;
        }
        s_cam.initialized = true;
    }

    /* 启动连续捕获，等待一帧就绪 */
    OV5640_JPEG_Start_Continuous();
    while (OV5640_FrameState == 0) { osDelay(1); }
    OV5640_FrameState = 0;

    OV5640_DCMI_Stop();
    __HAL_DCMI_CLEAR_FLAG(&hdcmi, DCMI_FLAG_FRAMERI);

    /* 在 DMA buffer 中查找 JPEG SOI (0xFF 0xD8) / EOI (0xFF 0xD9) */
    uint8_t *p = (uint8_t *)CAM_Buffer_ADDR;
    uint32_t jpg_start = 0, jpg_end = 0;
    bool     soi_found = false;
    uint32_t search_max = CAM_DMA_BufferSize * 4;

    for (uint32_t i = 0; i < search_max - 1; i++) {
        if (!soi_found && p[i] == 0xFF && p[i + 1] == 0xD8) {
            jpg_start = i;
            soi_found = true;
        }
        if (soi_found && p[i] == 0xFF && p[i + 1] == 0xD9) {
            jpg_end = i + 1;
            break;
        }
    }

    if (soi_found && jpg_end > jpg_start) {
        uint32_t jpg_size = jpg_end - jpg_start + 1;

        printf("===JPG %lu size:%lu fps:%d\r\n",
               (unsigned long)s_cam.frame_id + 1,
               (unsigned long)jpg_size,
               OV5640_FPS);

        if (jpg_size > UP_PAYLOAD_MAX) {
            printf("[ERR] JPEG too large, skip\r\n");
            osSemaphoreRelease(s_cam.sem);
            return;
        }

        s_cam.frame_id++;
        s_cam.frame.data_type = DATA_TYPE_IMG_FRAG;
        s_cam.frame.data_len  = (uint16_t)jpg_size;
        memcpy(s_cam.frame.data_buf, &p[jpg_start], jpg_size);
        s_cam.ready = true;

        /* 打印前 32 字节校验 JPEG 头部 */
        printf("  HDR:");
        for (uint32_t i = 0; i < 32 && i < jpg_size; i++)
            printf(" %02X", s_cam.frame.data_buf[i]);
        printf("\r\n");
        printf("  TAIL:");
        for (uint32_t i = (jpg_size > 32 ? jpg_size - 32 : 0); i < jpg_size; i++)
            printf(" %02X", s_cam.frame.data_buf[i]);
        printf("\r\n");
        printf("===END\r\n\r\n");
    } else {
        printf("[WARN] SOI/EOI not found, skip frame\r\n");
        osSemaphoreRelease(s_cam.sem);
    }
}

static bool     IsReady(void)      { return s_cam.ready; }
static UpFrame* GetFrame(void)     { return &s_cam.frame; }
static uint8_t  GetPrio(void)      { return 20; }

static void Flush(void)
{
    s_cam.ready = false;
    osSemaphoreRelease(s_cam.sem);
}

DataModule_t g_data_module_cam = {
    .init       = Init,
    .collect    = Collect,
    .is_ready   = IsReady,
    .get_frame  = GetFrame,
    .get_prio   = GetPrio,
    .flush      = Flush,
};

/* ====================== 采集任务（1 fps） ====================== */
void Cam_Collect_Task(void* argument)
{
    (void)argument;
    while (1) {
        g_data_module_cam.collect();
        osDelay(1000);
    }
}
