/**
 * ===========================================================================
 * 业务模块层 — 下行指令模块接口 + 注册表 + 分发器
 * ===========================================================================
 *
 * 新增指令步骤：
 *   1. 实现 CmdModule 接口（match + execute）
 *   2. 在 dispatcher_cmd.c 注册表中添加模块指针
 *
 * 严禁修改分发器与 BSP 驱动。
 */
#pragma once
#include "com_def.h"

typedef struct CmdModule {
    void (*init)(void);
    bool (*match)(uint8_t cmd_code);
    void (*execute)(const CmdMsg* cmd);
} CmdModule_t;

/* 注册表 */
extern CmdModule_t* const g_cmd_module_list[];
extern const uint32_t     g_cmd_module_num;

/* 分发器：遍历注册表，匹配 cmd_code 并执行 */
void CmdDispatcher_Process(const CmdMsg* cmd);
