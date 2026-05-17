/**
 * ===========================================================================
 * SPI 协议层实现
 * ===========================================================================
 */
#include "spi_protocol.h"
#include <string.h>

static DataModule_t* s_active_owner = NULL;

void SpiProto_SetActiveOwner(DataModule_t* owner) { s_active_owner = owner; }

/* ====================== MOSI 打包 ====================== */
uint16_t SpiProto_PackUpFrame(const UpFrame* f, uint8_t wire[SPI_WIRE_SIZE])
{
    wire[0] = DATA_VALID_REAL;
    wire[1] = f->data_type;
    wire[2] = (uint8_t)((f->data_len >> 8) & 0xFF);
    wire[3] = (uint8_t)(f->data_len & 0xFF);

    uint16_t copy_len = f->data_len;
    if (copy_len > UP_PAYLOAD_MAX) copy_len = UP_PAYLOAD_MAX;
    memcpy(&wire[UP_HDR_SIZE], f->data_buf, copy_len);

    return UP_HDR_SIZE + copy_len;
}

/* ====================== MISO 解析 ====================== */
static void ParseMiso(const uint8_t* rx, uint16_t len)
{
    if (!rx || len < CMD_HDR_SIZE) return;

    uint8_t flag = rx[0];
    uint8_t L    = rx[1];
    if (flag != CMD_FLAG_PENDING || L == 0 || L > CMD_DATA_MAX) return;
    if ((uint16_t)(CMD_HDR_SIZE + L) > len) return;

    const uint8_t* cmd_data = &rx[CMD_HDR_SIZE];
    if (L < 3) return;

    CmdMsg cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.cmd_code  = cmd_data[0];
    cmd.param_len = (uint16_t)cmd_data[1] | ((uint16_t)cmd_data[2] << 8);

    uint16_t avail = (uint16_t)(L - 3);
    if (cmd.param_len > avail)  cmd.param_len = avail;
    if (cmd.param_len > sizeof(cmd.param)) cmd.param_len = sizeof(cmd.param);
    memcpy(cmd.param, &cmd_data[3], cmd.param_len);

    CmdDispatcher_Process(&cmd);
}

/* ====================== BSP RX 回调（DMA 完成后 BSP 调用） ====================== */
static void RxIndication(const uint8_t* rx, uint16_t len, SpiWorkState state)
{
    (void)state;
    ParseMiso(rx, len);

    /* 释放本次发送的模块 buffer */
    if (s_active_owner && s_active_owner->flush) {
        s_active_owner->flush();
        s_active_owner = NULL;
    }
}

void SpiProto_RegisterAsRxCallback(void)
{
    BspSpi_RegisterRxCb(RxIndication);
}
