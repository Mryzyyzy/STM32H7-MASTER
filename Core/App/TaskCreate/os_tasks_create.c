/**
 * ===========================================================================
 * 应用调度层 — 任务创建实现
 * ===========================================================================
 *
 * 所有 FreeRTOS 线程在此统一创建。
 * 新增传感器时只需在此函数末尾追加 osThreadNew() 调用。
 */
#include "os_tasks_create.h"
#include "spi_master.h"
#include "module_test.h"
#include "com_def.h"
#include "cmsis_os2.h"

void OsTasks_Create(void)
{
    /* ---- SPI 主调度任务（10ms，最高优先级） ---- */
    const osThreadAttr_t spiMain_attr = {
        .name       = "spiMain",
        .stack_size = 512,
        .priority   = PRIO_SPI_MAIN,
    };
    osThreadNew(SPI_Main_Task, NULL, &spiMain_attr);

    /* ---- 测试模块采集任务（10Hz） ---- */
    const osThreadAttr_t test_attr = {
        .name       = "testTask",
        .stack_size = 256,
        .priority   = PRIO_SENSOR_LOW,
    };
    osThreadNew(Test_Task, NULL, &test_attr);

    /*
     * 新增传感器任务示例：
     *
     * extern void Img_Task(void* argument);
     * const osThreadAttr_t img_attr = {
     *     .name = "imgTask", .stack_size = 1024, .priority = PRIO_SENSOR_HIGH,
     * };
     * osThreadNew(Img_Task, NULL, &img_attr);
     */
}
