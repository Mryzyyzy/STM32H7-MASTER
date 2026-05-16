/**
 * ===========================================================================
 * 摄像头采集模块（RGB565 连续模式 — 和官方例程一致）
 * ===========================================================================
 */
#include "cam_module.h"
#include "bsp_cam.h"
#include "bsp_lcd.h"
#include "cmsis_os2.h"
#include <stdio.h>

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
