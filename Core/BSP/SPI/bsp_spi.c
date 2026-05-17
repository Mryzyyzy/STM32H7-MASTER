/**
 * ===========================================================================
 * BSP 驱动层 — SPI 全双工 DMA 驱动实现
 * ===========================================================================
 */
#include "bsp_spi.h"
#include <string.h>

/* ====================== CS 引脚 ====================== */
#define SPI_CS_PORT     GPIOC
#define SPI_CS_PIN      GPIO_PIN_3
#define SPI_CS_LOW()    HAL_GPIO_WritePin(SPI_CS_PORT, SPI_CS_PIN, GPIO_PIN_RESET)
#define SPI_CS_HIGH()   HAL_GPIO_WritePin(SPI_CS_PORT, SPI_CS_PIN, GPIO_PIN_SET)

/* ====================== 全局状态 ====================== */
volatile SpiWorkState g_spi_state = SPI_IDLE;
volatile bool         g_spi_busy  = false;

/* ====================== 内部资源 ====================== */
static SPI_HandleTypeDef*  s_hspi     = NULL;
static BspSpi_RxCallback   s_rx_cb    = NULL;
static uint16_t            s_xfer_len = 0;

/* DMA 缓冲区必须在 AXI SRAM (.dma_bss)，STM32H7 DMA1 无法访问 DTCMRAM */
__attribute__((section(".dma_bss"), aligned(32)))
static uint8_t s_tx_buf[SPI_WIRE_SIZE];

__attribute__((section(".dma_bss"), aligned(32)))
static uint8_t s_rx_buf[SPI_WIRE_SIZE];

__attribute__((section(".dma_bss"), aligned(32)))
static uint8_t s_dummy_buf[SPI_WIRE_SIZE];

/* ====================== 初始化 ====================== */
void BspSpi_Init(SPI_HandleTypeDef* hspi)
{
    s_hspi = hspi;
    memset(s_dummy_buf, 0, sizeof(s_dummy_buf));
    s_dummy_buf[0] = DATA_VALID_DUMMY;
    s_dummy_buf[1] = DATA_TYPE_DUMMY;
    SPI_CS_HIGH();
}

void BspSpi_RegisterRxCb(BspSpi_RxCallback cb) { s_rx_cb = cb; }

/* ====================== 全双工 DMA ====================== */
HAL_StatusTypeDef BspSpi_StartFullDuplex(uint8_t* tx_ptr, uint16_t len)
{
    if (!s_hspi || g_spi_busy)   return HAL_ERROR;
    if (len > SPI_WIRE_SIZE)     return HAL_ERROR;
    if (len == 0)                return HAL_ERROR;

    s_xfer_len = len;
    memcpy(s_tx_buf, tx_ptr, len);

    g_spi_busy = true;
    SPI_CS_LOW();
    return HAL_SPI_TransmitReceive_DMA(s_hspi, s_tx_buf, s_rx_buf, len);
}

HAL_StatusTypeDef BspSpi_StartPoll(void)
{
    g_spi_state = SPI_TX_POLL;
    return BspSpi_StartFullDuplex(s_dummy_buf, UP_HDR_SIZE);
}

/* ====================== DMA 完成回调 (weak override) ====================== */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef* hspi)
{
    if (hspi != s_hspi) return;
    SPI_CS_HIGH();

    if (s_rx_cb) s_rx_cb(s_rx_buf, s_xfer_len, g_spi_state);

    g_spi_busy  = false;
    g_spi_state = SPI_IDLE;
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef* hspi)
{
    if (hspi != s_hspi) return;
    SPI_CS_HIGH();
    g_spi_busy  = false;
    g_spi_state = SPI_IDLE;
}
