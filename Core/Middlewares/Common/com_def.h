/**
 * ===========================================================================
 * 基础定义层 — 协议常量 / 公共类型 / 全局状态（全局唯一）
 * ===========================================================================
 *
 * 适配 ESP32 从机通信格式：
 *   MOSI [data_valid(1) | data_type(1) | payload_len(2,BE) | payload]
 *   MISO [cmd_flag(1)  | cmd_len(1)  | cmd_data(N)]
 *   IRQ  电平：HIGH=ESP32 就绪  LOW=缓冲耗尽
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ====================== SPI 传输常量 ====================== */
#define SPI_WIRE_SIZE           (1024U)
#define UP_HDR_SIZE             (4U)            /* data_valid + data_type + payload_len(BE) */
#define UP_PAYLOAD_MAX          (SPI_WIRE_SIZE - UP_HDR_SIZE)      /* 1020 */
#define CMD_HDR_SIZE            (2U)            /* cmd_flag + cmd_len */
#define CMD_DATA_MAX            (255U)

/* ====================== MOSI 上行帧头 ====================== */
#define DATA_VALID_DUMMY        (0x00U)
#define DATA_VALID_REAL         (0x01U)

#define DATA_TYPE_DUMMY         (0x00U)
#define DATA_TYPE_IMG_FRAG      (0x01U)
#define DATA_TYPE_IMG_LAST      (0x02U)
#define DATA_TYPE_TEMP          (0x10U)

/* ====================== MISO 下行响应 ====================== */
#define CMD_FLAG_NONE           (0x00U)
#define CMD_FLAG_PENDING        (0x01U)

/* ====================== DMA 对齐 ====================== */
#define DMA_ALIGNED4            __attribute__((aligned(4)))

/* ====================== 上行帧（模块内部使用） ====================== */
typedef struct DMA_ALIGNED4 {
    uint8_t  data_type;
    uint16_t data_len;
    uint8_t  data_buf[UP_PAYLOAD_MAX];
} UpFrame;

/* ====================== 下行命令（解析后投递） ====================== */
typedef struct {
    uint8_t  cmd_code;
    uint8_t  param_len;
    uint8_t  param[CMD_DATA_MAX - 1];
} CmdMsg;

/* ====================== SPI 状态机 ====================== */
typedef enum {
    SPI_IDLE = 0,
    SPI_TX_NORMAL,
    SPI_TX_POLL,
} SpiWorkState;

/* ====================== 全局状态（BSP 驱动层维护） ====================== */
extern volatile SpiWorkState g_spi_state;
extern volatile bool         g_spi_busy;

/* ====================== 任务优先级 ====================== */
#define PRIO_SPI_MAIN       (osPriorityAboveNormal)
#define PRIO_SENSOR_HIGH    (osPriorityNormal)
#define PRIO_SENSOR_LOW     (osPriorityLow)
