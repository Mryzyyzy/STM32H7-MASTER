/**
 * ===========================================================================
 * 摄像头采集模块（RGB565 / JPEG 两种模式）
 * ===========================================================================
 */
#include "cam_module.h"
#include "bsp_cam.h"
#include "bsp_lcd.h"
#include "cmsis_os2.h"
#include <stdio.h>

/* ---- RGB565 连续模式，LCD 显示 ---- */
void Camera_Task(void *argument)
{
    (void)argument;

    SPI_LCD_Init();
    LCD_DisplayString(10, 130, "Waiting...");

    if (DCMI_OV5640_Init() != OV5640_Success) {
        LCD_DisplayString(10, 130, "Cam Error!");
        while (1) osDelay(1000);
    }

    OV5640_DMA_Transmit_Continuous(CAM_Buffer, CAM_DMA_BufferSize);

    while (1) {
        if (OV5640_FrameState == 1) {
            OV5640_FrameState = 0;

            LCD_CopyBuffer(0, 0, CAM_DISPLAY_Width, CAM_DISPLAY_Height,
                           (uint16_t *)(uintptr_t)CAM_Buffer_ADDR);

            char buf[16];
            sprintf(buf, "fps:%d", OV5640_FPS);
            LCD_DisplayString(34, 252, buf);
        }
        osDelay(1);
    }
}

/* ---- JPEG 快照模式，Base64 编码后串口输出（每秒一帧） ---- */

static const char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* 将 JPEG 数据按 Base64 编码后通过 printf 输出，每行最多 128 字符 */
static void print_base64_jpg(const uint8_t *data, uint32_t len)
{
    uint32_t col = 0;
    for (uint32_t i = 0; i < len; i += 3) {
        uint32_t triple = ((uint32_t)data[i]) << 16;
        if (i + 1 < len) triple |= ((uint32_t)data[i + 1]) << 8;
        if (i + 2 < len) triple |= data[i + 2];

        printf("%c%c%c%c",
               base64_table[(triple >> 18) & 0x3F],
               base64_table[(triple >> 12) & 0x3F],
               (i + 1 < len) ? base64_table[(triple >> 6) & 0x3F] : '=',
               (i + 2 < len) ? base64_table[triple & 0x3F] : '=');

        col += 4;
        if (col >= 128) { printf("\r\n"); col = 0; }
    }
    if (col > 0) printf("\r\n");
}

void Camera_JPEG_Task(void *argument)
{
    (void)argument;

    printf("\r\n===== OV5640 JPEG Serial Test =====\r\n");
    printf("Format: Base64, %dx%d\r\n",
           CAM_DISPLAY_Width, CAM_DISPLAY_Height);

    if (DCMI_OV5640_JPEG_Init() != OV5640_Success) {
        printf("[ERR] JPEG init failed!\r\n");
        while (1) osDelay(1000);
    }
    printf("[OK] JPEG init done\r\n");

    uint32_t frame_count = 0;

    while (1) {
        OV5640_JPEG_Start_Continuous();

        while (OV5640_FrameState == 0) {
            osDelay(1);
        }
        OV5640_FrameState = 0;
        frame_count++;

        OV5640_DCMI_Stop();
        __HAL_DCMI_CLEAR_FLAG(&hdcmi, DCMI_FLAG_FRAMERI);

        uint8_t *p = (uint8_t *)CAM_Buffer_ADDR;
        uint32_t jpg_start = 0, jpg_end = 0;
        uint8_t  soi_found = 0;
        uint32_t search_max = CAM_DMA_BufferSize * 4;

        for (uint32_t i = 0; i < search_max - 1; i++) {
            if (!soi_found && p[i] == 0xFF && p[i + 1] == 0xD8) {
                jpg_start = i;
                soi_found = 1;
            }
            if (soi_found && p[i] == 0xFF && p[i + 1] == 0xD9) {
                jpg_end = i + 1;
                break;
            }
        }

        if (soi_found && jpg_end > jpg_start) {
            uint32_t jpg_size = jpg_end - jpg_start + 1;

            printf("===JPG %lu %lu fps:%d\r\n",
                   (unsigned long)frame_count,
                   (unsigned long)jpg_size,
                   OV5640_FPS);
            print_base64_jpg(&p[jpg_start], jpg_size);
            printf("===END\r\n\r\n");
        } else {
            printf("===JPG %lu 0\r\n===END\r\n\r\n",
                   (unsigned long)frame_count);
        }

        osDelay(1000);
    }
}
