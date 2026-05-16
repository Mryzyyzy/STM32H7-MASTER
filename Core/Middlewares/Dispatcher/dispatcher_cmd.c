/**
 * ===========================================================================
 * 业务模块层 — 下行指令分发器 + 注册表
 * ===========================================================================
 */
#include "dispatcher_cmd.h"

void CmdDispatcher_Process(const CmdMsg* cmd)
{
    if (!cmd) return;
    for (uint32_t i = 0; i < g_cmd_module_num; i++) {
        CmdModule_t* m = g_cmd_module_list[i];
        if (m && m->match && m->match(cmd->cmd_code)) {
            if (m->execute) m->execute(cmd);
            return;
        }
    }
}

/* ====================== 注册表 ====================== */
CmdModule_t* const g_cmd_module_list[] = {
    /* 在此添加指令模块：&g_cmd_module_led, ... */
};

const uint32_t g_cmd_module_num = sizeof(g_cmd_module_list) / sizeof(g_cmd_module_list[0]);
