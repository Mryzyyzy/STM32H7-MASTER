/**
 * ===========================================================================
 * 业务模块层 — 摄像头采集模块
 * ===========================================================================
 *
 * 摄像头任务：支持 RGB565+LCD 显示 和 JPEG+SPI 上行 两种模式
 */
#pragma once

#include "scheduler_data.h"

/* 摄像头主任务（初始化 LCD + OV5640 + 主循环显示） */
void Camera_Task(void *argument);

/* JPEG 采集模块对象（通过 SPI 调度器上行发送） */
extern DataModule_t g_data_module_cam;

/* JPEG 采集任务（1 fps，调用 Collect 采集后由 SPI 调度器发送） */
void Cam_Collect_Task(void* argument);
