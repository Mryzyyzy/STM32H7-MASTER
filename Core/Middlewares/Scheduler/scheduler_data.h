/**
 * ===========================================================================
 * 业务模块层 — 上行数据模块接口 + 注册表 + 调度器
 * ===========================================================================
 *
 * 新增传感器步骤：
 *   1. 实现 DataModule 接口（见 module_test.c 示例）
 *   2. 在 scheduler_data.c 注册表中添加模块指针
 *   3. 在 App/SpiMaster/spi_master.c 编写采集任务函数
 *   4. 在 App/TaskCreate/os_tasks_create.c 创建该任务
 *
 * 严禁修改调度器与 BSP 驱动。
 */
#pragma once
#include "com_def.h"

typedef struct DataModule {
    void     (*init)(void);
    void     (*collect)(void);
    bool     (*is_ready)(void);
    UpFrame* (*get_frame)(void);
    uint8_t  (*get_prio)(void);
    void     (*flush)(void);
} DataModule_t;

/* 注册表 */
extern DataModule_t* const g_data_module_list[];
extern const uint32_t      g_data_module_num;

/* 调度器：返回优先级最高且 ready 的帧，out_owner 用于发送完成后 flush */
UpFrame* DataScheduler_GetHighestPrioFrame(DataModule_t** out_owner);
