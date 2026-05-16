/**
 * ===========================================================================
 * 业务模块层 — 上行数据调度器 + 注册表
 * ===========================================================================
 */
#include "scheduler_data.h"

UpFrame* DataScheduler_GetHighestPrioFrame(DataModule_t** out_owner)
{
    DataModule_t* best_m = NULL;
    UpFrame*      best_f = NULL;
    uint8_t       best_p = 0;

    for (uint32_t i = 0; i < g_data_module_num; i++) {
        DataModule_t* m = g_data_module_list[i];
        if (m && m->is_ready && m->is_ready()) {
            uint8_t p = m->get_prio ? m->get_prio() : 0;
            if (!best_m || p > best_p) {
                best_m = m;
                best_f = m->get_frame ? m->get_frame() : NULL;
                best_p = p;
            }
        }
    }
    if (out_owner) *out_owner = best_m;
    return best_f;
}

/* ====================== 注册表 ====================== */
extern DataModule_t g_data_module_test;

/* ===== 注册表 — cam 模块暂时独立运行（不通过 SPI 调度器发送） ===== */
DataModule_t* const g_data_module_list[] = {
    &g_data_module_test,
    /* 后续添加 cam 分片发送模块：&g_data_module_cam, */
    /* 在此继续添加：&g_data_module_img, &g_data_module_temp, ... */
};

const uint32_t g_data_module_num = sizeof(g_data_module_list) / sizeof(g_data_module_list[0]);
