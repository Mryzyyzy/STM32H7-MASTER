/***
1.69寸LCD 配置函数
***/

#include "bsp_lcd.h"
#include "main.h"
#include <stdio.h>

SPI_HandleTypeDef hspi1;         // SPI_HandleTypeDef 结构体变量

#define LCD_SPI hspi1             // SPI局部宏，方便修改和移植

static pFONT *LCD_AsciiFonts;    // 英文字体，ASCII字符集
static pFONT *LCD_CHFonts;       // 中文字体（同时也包含英文字体）

// 为了提高字符显示速度，对于显存的写入，需要将整个字符像素的一次性写入，而不是一个像素的写入
// 所以需要开辟一个字符显示缓冲区
// 用户根据实际情况去修改此处缓冲区的大小，
// 比如用户需要显示32*32点阵的汉字时，需要的大小为 32*32*2 = 2048 字节（每个像素点占2字节）
uint16_t  LCD_Buff[1024];        // LCD缓冲区，16位宽（每个像素点占2字节）

struct
{
    uint32_t Color;              // 画笔颜色
    uint32_t BkgColor;           // 背景颜色
    uint8_t  Direction;          // 显示方向
    uint16_t Width;              // 屏幕像素宽度
    uint16_t Height;             // 屏幕像素高度
    uint8_t  X_Offset;           // X坐标偏移
    uint8_t  Y_Offset;           // Y坐标偏移
}TFT_Ptr;  // 液晶参数结构

// 该函数修改于HAL的SPI库函数，原函数有写入长度限制，所以修改为SPI传输数据不限数据长度的写入
static HAL_StatusTypeDef LCD_SPI_Transmit(SPI_HandleTypeDef *hspi, uint16_t pData, uint32_t Size);
static HAL_StatusTypeDef LCD_SPI_TransmitBuffer(SPI_HandleTypeDef *hspi, uint16_t *pData, uint32_t Size);
static void               LCD_SPI_CloseTransfer(SPI_HandleTypeDef *hspi);
static HAL_StatusTypeDef LCD_SPI_WaitOnFlagUntilTimeout(SPI_HandleTypeDef *hspi,
                                                         uint32_t Flag, FlagStatus Status,
                                                         uint32_t Tickstart, uint32_t Timeout);

/****************************************************************************************************************************************
*   函 数 名: HAL_SPI_MspInit
*   入口参数: hspi - SPI_HandleTypeDef定义的变量，即表示定义的 SPI 句柄
*   函数功能: 初始化 SPI 引脚
*   说    明: SPI1 初始化时会重新配置 PLL2（150MHz），SPI2 也会受此影响
****************************************************************************************************************************************/

void HAL_SPI_MspInit(SPI_HandleTypeDef* hspi)
{
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hspi->Instance == SPI2) {
        /* SPI2 clock enable — PLL2 already configured by SPI1 init */
        __HAL_RCC_SPI2_CLK_ENABLE();

        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        /**SPI2 GPIO Configuration
        PC1     ------> SPI2_MOSI
        PC2_C     ------> SPI2_MISO
        PB13     ------> SPI2_SCK
        */
        GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_13;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        /* SPI2 DMA Init */
        extern DMA_HandleTypeDef hdma_spi2_rx;
        extern DMA_HandleTypeDef hdma_spi2_tx;

        /* SPI2_RX Init */
        hdma_spi2_rx.Instance = DMA1_Stream0;
        hdma_spi2_rx.Init.Request = DMA_REQUEST_SPI2_RX;
        hdma_spi2_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
        hdma_spi2_rx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_spi2_rx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_spi2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_spi2_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_spi2_rx.Init.Mode = DMA_NORMAL;
        hdma_spi2_rx.Init.Priority = DMA_PRIORITY_LOW;
        hdma_spi2_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        if (HAL_DMA_Init(&hdma_spi2_rx) != HAL_OK) {
            Error_Handler();
        }
        __HAL_LINKDMA(hspi, hdmarx, hdma_spi2_rx);

        /* SPI2_TX Init */
        hdma_spi2_tx.Instance = DMA1_Stream1;
        hdma_spi2_tx.Init.Request = DMA_REQUEST_SPI2_TX;
        hdma_spi2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
        hdma_spi2_tx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_spi2_tx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_spi2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_spi2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_spi2_tx.Init.Mode = DMA_NORMAL;
        hdma_spi2_tx.Init.Priority = DMA_PRIORITY_LOW;
        hdma_spi2_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        if (HAL_DMA_Init(&hdma_spi2_tx) != HAL_OK) {
            Error_Handler();
        }
        __HAL_LINKDMA(hspi, hdmatx, hdma_spi2_tx);

        /* SPI2 interrupt Init */
        HAL_NVIC_SetPriority(SPI2_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(SPI2_IRQn);
    }

    if (hspi->Instance == SPI1) {
        // PLL2: 24MHz / 16 * 100 / 1 = 150MHz → SPI1 SCK 150/2 = 75MHz
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SPI1;
        PeriphClkInitStruct.PLL2.PLL2M = 16;
        PeriphClkInitStruct.PLL2.PLL2N = 100;
        PeriphClkInitStruct.PLL2.PLL2P = 1;
        PeriphClkInitStruct.PLL2.PLL2Q = 2;
        PeriphClkInitStruct.PLL2.PLL2R = 2;
        PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_0;
        PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOMEDIUM;
        PeriphClkInitStruct.PLL2.PLL2FRACN = 0;
        PeriphClkInitStruct.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL2;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
            Error_Handler();
        }
        __HAL_RCC_SPI1_CLK_ENABLE();         // 使能SPI1时钟
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();

        /**SPI1 GPIO Configuration
            PD7  ------> SPI1_MOSI
            PB3  ------> SPI1_SCK
            PB4  ------> SPI1_MISO
        */
        GPIO_InitStruct.Pin = GPIO_PIN_7;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
        HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_4;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        /*PA15 SPI1 液晶屏片选 */
        GPIO_InitStruct.Pin = GPIO_PIN_15;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);

        /*PC4 液晶SPI C/D  */
        /*PC13 液晶SPI RST  */
        GPIO_InitStruct.Pin = GPIO_PIN_13 | GPIO_PIN_4;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        /*PA7 液晶背光控制*/
        GPIO_InitStruct.Pin = GPIO_PIN_7;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);   // 打开液晶背光
    }
}

/****************************************************************************************************************************************
*   函 数 名: MX_SPI1_Init
*   函数功能: 初始化SPI配置
*   说    明: 使用软件片选
****************************************************************************************************************************************/

void MX_SPI1_Init(void)
{
    LCD_SPI.Instance                    = SPI1;                              // 使用SPI1
    LCD_SPI.Init.Mode                   = SPI_MODE_MASTER;                   // 主机模式
    LCD_SPI.Init.Direction              = SPI_DIRECTION_1LINE;               // 单线
    LCD_SPI.Init.DataSize               = SPI_DATASIZE_8BIT;                 // 8位数据宽度
    LCD_SPI.Init.CLKPolarity            = SPI_POLARITY_LOW;                  // CLK空闲时保持低电平
    LCD_SPI.Init.CLKPhase               = SPI_PHASE_1EDGE;                   // 数据在CLK第一个边沿有效
    LCD_SPI.Init.NSS                    = SPI_NSS_SOFT;                      // 使用软件片选

    // SPI外设时钟为100M，经过2分频得到50M 的SCK时钟
    LCD_SPI.Init.BaudRatePrescaler      = SPI_BAUDRATEPRESCALER_2;

    LCD_SPI.Init.FirstBit               = SPI_FIRSTBIT_MSB;                  // 高位在先
    LCD_SPI.Init.TIMode                 = SPI_TIMODE_DISABLE;                // 是为了确保 SPI 接口以标准模式运行，从而与非 TI 设备进行兼容的数据传输
    LCD_SPI.Init.CRCCalculation         = SPI_CRCCALCULATION_DISABLE;        // 禁止CRC
    LCD_SPI.Init.CRCPolynomial          = 0x0;                               // CRC校验项
    LCD_SPI.Init.NSSPMode               = SPI_NSS_PULSE_ENABLE;              // 使用片选脉冲模式
    LCD_SPI.Init.NSSPolarity            = SPI_NSS_POLARITY_LOW;              // 片选低电平有效
    LCD_SPI.Init.FifoThreshold          = SPI_FIFO_THRESHOLD_02DATA;         // FIFO阈值
    LCD_SPI.Init.TxCRCInitializationPattern  = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;   // 发送端CRC初始化模式，这里用不到
    LCD_SPI.Init.RxCRCInitializationPattern  = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;   // 接收端CRC初始化模式，这里用不到
    LCD_SPI.Init.MasterSSIdleness            = SPI_MASTER_SS_IDLENESS_00CYCLE;            // 额外延迟周期为0
    LCD_SPI.Init.MasterInterDataIdleness     = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;     // 主机模式下，两个数据帧之间的延迟周期
    LCD_SPI.Init.MasterReceiverAutoSusp      = SPI_MASTER_RX_AUTOSUSP_DISABLE;            // 禁止自动接收管理
    LCD_SPI.Init.MasterKeepIOState           = SPI_MASTER_KEEP_IO_STATE_DISABLE;          // 主机模式下，禁止SPI保持当前引脚状态
    LCD_SPI.Init.IOSwap                      = SPI_IO_SWAP_DISABLE;                       // 不交换MOSI和MISO

    HAL_SPI_Init(&LCD_SPI);
}

/****************************************************************************************************************************************
*   函 数 名: TFT_SEND_CMD
*   入口参数: 需要写入的命令
*   函数功能: 给液晶屏幕控制器写入命令
****************************************************************************************************************************************/

void  TFT_SEND_CMD(uint8_t lcd_command)
{
    TFT_DC_C;     // 拉低DC引脚：命令传输
    LCD_CSL;      // 拉低CS引脚，开始传输
    HAL_SPI_Transmit(&LCD_SPI, &lcd_command, 1, 1000); // 启动SPI传输
    LCD_CSH;      // 拉高CS引脚,传输结束
}

/****************************************************************************************************************************************
*   函 数 名: TFT_SEND_DATA
*   入口参数: lcd_data - 需要写入的数据，8位
*   函数功能: 写入8位数据
****************************************************************************************************************************************/

void  TFT_SEND_DATA(uint8_t lcd_data)
{
    TFT_DC_D;     // 拉高DC引脚：数据传输
    LCD_CSL;      // 拉低CS引脚，开始传输
    HAL_SPI_Transmit(&LCD_SPI, &lcd_data, 1, 1000); // 启动SPI传输
    LCD_CSH;      // 拉高CS引脚，传输结束
}

/****************************************************************************************************************************************
*   函 数 名: TFT_SEND_DATA_16b
*   入口参数: lcd_data - 需要写入的数据，16位
*   函数功能: 写入16位数据
****************************************************************************************************************************************/

void  TFT_SEND_DATA_16b(uint16_t lcd_data)
{
    uint8_t lcd_data_buff[2];   // 数据发送缓存区
    TFT_DC_D;                   // 拉高DC引脚：数据传输
    LCD_CSL;                    // 拉低CS引脚，开始传输
    lcd_data_buff[0] = lcd_data >> 8;  // 数据拆分
    lcd_data_buff[1] = lcd_data;

    HAL_SPI_Transmit(&LCD_SPI, lcd_data_buff, 2, 1000);   // 启动SPI传输
    LCD_CSH;  // 拉高CS引脚,传输结束
}

/****************************************************************************************************************************************
*   函 数 名: LCD_WriteBuff
*   入口参数: DataBuff - 待发送数据  DataSize - 数据长度
*   函数功能: 批量写入数据到屏幕
****************************************************************************************************************************************/

void  LCD_WriteBuff(uint16_t *DataBuff, uint16_t DataSize)
{
    TFT_DC_D;     // 拉高DC引脚：数据传输
    LCD_CSL;      // 拉低CS引脚，开始传输
    // 改为16位数据宽度，提高效率
    LCD_SPI.Init.DataSize = SPI_DATASIZE_16BIT;   // 16位数据宽度
    HAL_SPI_Init(&LCD_SPI);
    HAL_SPI_Transmit(&LCD_SPI, (uint8_t *)DataBuff, DataSize, 1000); // 启动SPI传输
    LCD_CSH;      // 拉高CS引脚,传输结束
    // 改回8位数据宽度，因为指令和参数数据都是按8位传输的
    LCD_SPI.Init.DataSize = SPI_DATASIZE_8BIT;    // 8位数据宽度
    HAL_SPI_Init(&LCD_SPI);
}

/****************************************************************************************************************************************
*   函 数 名: SPI_LCD_Init
*   函数功能: 初始化SPI以及屏幕控制器的各种配置
****************************************************************************************************************************************/

void SPI_LCD_Init(void)
{
    MX_SPI1_Init();              // 初始化SPI和控制器
    LCD_RST_L;                   // RST 拉低
    HAL_Delay(10);
    LCD_RST_H;                   // RST 拉高  完成对屏幕的复位
    HAL_Delay(10);               // 屏幕内部复位，需要等待超过5ms才能发送指令

    TFT_SEND_CMD(0x36);          // 设置屏幕LCD的显示方向、RGB颜色格式（RGB/BGR）
    // 设置成"从上至下、从左至右，RGB像素格式"
    TFT_SEND_DATA(0x00);         // 参数
    TFT_SEND_CMD(0x3A);          // 接口像素格式指令，可配置使用12位/16位/18位色
    TFT_SEND_DATA(0x05);         // 此处设置成 16位 像素格式

    // 下面的电压配置指令，采用芯片默认值
    TFT_SEND_CMD(0xB2);
    TFT_SEND_DATA(0x0C);
    TFT_SEND_DATA(0x0C);
    TFT_SEND_DATA(0x00);
    TFT_SEND_DATA(0x33);
    TFT_SEND_DATA(0x33);

    TFT_SEND_CMD(0xB7);          // VGH/VGL 电压配置指令
    TFT_SEND_DATA(0x35);

    TFT_SEND_CMD(0xBB);          // VCOM 电压配置指令
    TFT_SEND_DATA(0x19);

    TFT_SEND_CMD(0xC0);
    TFT_SEND_DATA(0x2C);

    TFT_SEND_CMD(0xC2);
    TFT_SEND_DATA(0x01);

    TFT_SEND_CMD(0xC3);          // VRH电压 配置指令
    TFT_SEND_DATA(0x12);

    TFT_SEND_CMD(0xC4);          // VDV电压 配置指令
    TFT_SEND_DATA(0x20);

    TFT_SEND_CMD(0xC6);          // 显示模式下帧率控制指令
    TFT_SEND_DATA(0x0F);         // 设置屏幕默认刷新帧率为60帧

    TFT_SEND_CMD(0xD0);          // 电源控制指令
    TFT_SEND_DATA(0xA4);
    TFT_SEND_DATA(0xA1);

    TFT_SEND_CMD(0xE0);          // 正电压伽马值设定
    TFT_SEND_DATA(0xD0);
    TFT_SEND_DATA(0x04);
    TFT_SEND_DATA(0x0D);
    TFT_SEND_DATA(0x11);
    TFT_SEND_DATA(0x13);
    TFT_SEND_DATA(0x2B);
    TFT_SEND_DATA(0x3F);
    TFT_SEND_DATA(0x54);
    TFT_SEND_DATA(0x4C);
    TFT_SEND_DATA(0x18);
    TFT_SEND_DATA(0x0D);
    TFT_SEND_DATA(0x0B);
    TFT_SEND_DATA(0x1F);
    TFT_SEND_DATA(0x23);

    TFT_SEND_CMD(0xE1);          // 负电压伽马值设定
    TFT_SEND_DATA(0xD0);
    TFT_SEND_DATA(0x04);
    TFT_SEND_DATA(0x0C);
    TFT_SEND_DATA(0x11);
    TFT_SEND_DATA(0x13);
    TFT_SEND_DATA(0x2C);
    TFT_SEND_DATA(0x3F);
    TFT_SEND_DATA(0x44);
    TFT_SEND_DATA(0x51);
    TFT_SEND_DATA(0x2F);
    TFT_SEND_DATA(0x1F);
    TFT_SEND_DATA(0x1F);
    TFT_SEND_DATA(0x20);
    TFT_SEND_DATA(0x23);

    TFT_SEND_CMD(0x21);          // 打开反转，因为屏幕为常黑型，所以需要这个配置

    // 退出休眠指令，LCD在刚上电、复位时，会自动进入休眠模式，所以使用屏幕之前需要退出休眠
    TFT_SEND_CMD(0x11);          // 退出休眠 指令
    HAL_Delay(120);              // 需要等待120ms，等电源电压和时钟电路稳定

    // 开显示指令，LCD在刚上电、复位时，会自动关闭显示
    TFT_SEND_CMD(0x29);          // 开显示

    // 下面进行一些基本的默认配置
    LCD_SetDirection(Direction_V);         // 设置显示方向
    LCD_SetBackColor(LCD_BLACK);           // 设置背景色
    LCD_SetColor(LCD_WHITE);               // 设置画笔色
    LCD_Clear();                           // 清屏

    LCD_SetAsciiFont(&ASCII_Font24);       // 设置默认字体

    // 全部配置完成之后，打开背光
    LCD_Bkglight_ON;  // 输出高电平，开启背光
}

/****************************************************************************************************************************************
*   函 数 名:  LCD_SetAddress
*   入口参数:  x1 - 起始水平坐标    y1 - 起始垂直坐标
*              x2 - 终点水平坐标    y2 - 终点垂直坐标
*   函数功能:  设置要显示的内存区域
*****************************************************************************************************************************************/

void LCD_SetAddress(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    TFT_SEND_CMD(0x2a);       // 列地址设置，即X坐标
    TFT_SEND_DATA_16b(x1 + TFT_Ptr.X_Offset);
    TFT_SEND_DATA_16b(x2 + TFT_Ptr.X_Offset);

    TFT_SEND_CMD(0x2b);       // 行地址设置，即Y坐标
    TFT_SEND_DATA_16b(y1 + TFT_Ptr.Y_Offset);
    TFT_SEND_DATA_16b(y2 + TFT_Ptr.Y_Offset);

    TFT_SEND_CMD(0x2c);       // 开始写显存，接下来要显示的颜色数据
}

/****************************************************************************************************************************************
*   函 数 名: LCD_SetColor
*   入口参数: Color - 要显示的颜色，例如0x0000FF 表示蓝色
*   函数功能: 此函数用于设置画笔的颜色，用于显示字符、画点、画线、画图等
*   说    明: 1. 为了方便用户使用自定义颜色，入口参数 Color 使用24位 RGB888 颜色格式
*             2. 24位颜色中，从高位到低位分别对应 R、G、B  3个颜色通道
*****************************************************************************************************************************************/

void LCD_SetColor(uint32_t Color)
{
    uint16_t Red_Value = 0, Green_Value = 0, Blue_Value = 0; // 三个颜色通道的值

    Red_Value   = (uint16_t)((Color & 0x00F80000) >> 8);   // 转换成 16位 RGB565颜色
    Green_Value = (uint16_t)((Color & 0x0000FC00) >> 5);
    Blue_Value  = (uint16_t)((Color & 0x000000F8) >> 3);

    TFT_Ptr.Color = (uint16_t)(Red_Value | Green_Value | Blue_Value);  // 将颜色写入全局LCD变量
}

/****************************************************************************************************************************************
*   函 数 名: LCD_SetBackColor
*   入口参数: Color - 要显示的颜色，例如0x0000FF 表示蓝色
*   函数功能: 设置背景色,此函数用于设定自己显示字符的背景色
*   说    明: 同 LCD_SetColor
*****************************************************************************************************************************************/

void LCD_SetBackColor(uint32_t Color)
{
    uint16_t Red_Value = 0, Green_Value = 0, Blue_Value = 0;

    Red_Value   = (uint16_t)((Color & 0x00F80000) >> 8);
    Green_Value = (uint16_t)((Color & 0x0000FC00) >> 5);
    Blue_Value  = (uint16_t)((Color & 0x000000F8) >> 3);

    TFT_Ptr.BkgColor = (uint16_t)(Red_Value | Green_Value | Blue_Value);
}

/****************************************************************************************************************************************
*   函 数 名: LCD_SetDirection
*   入口参数: direction - 要显示的方向
*   函数功能: 设置要显示的方向
*   说    明: 1. 可选参数 Direction_H / Direction_V / Direction_H_Flip / Direction_V_Flip
*             2. 使用示例 LCD_DisplayDirection(Direction_H) 设置屏幕横屏显示
*****************************************************************************************************************************************/

void LCD_SetDirection(uint8_t direction)
{
    TFT_Ptr.Direction = direction;    // 写入全局LCD变量

    if (direction == Direction_H) {
        TFT_SEND_CMD(0x36);         // 显存读写方向控制指令，用来设置方向
        TFT_SEND_DATA(0x70);        // 横屏显示
        TFT_Ptr.X_Offset = 20;     // 设置可视区域坐标偏移量
        TFT_Ptr.Y_Offset = 0;
        TFT_Ptr.Width    = LCD_Height;       // 重新赋值宽高
        TFT_Ptr.Height   = LCD_Width;
    } else if (direction == Direction_V) {
        TFT_SEND_CMD(0x36);
        TFT_SEND_DATA(0x00);        // 竖屏显示
        TFT_Ptr.X_Offset = 0;
        TFT_Ptr.Y_Offset = 20;
        TFT_Ptr.Width    = LCD_Width;
        TFT_Ptr.Height   = LCD_Height;
    } else if (direction == Direction_H_Flip) {
        TFT_SEND_CMD(0x36);
        TFT_SEND_DATA(0xA0);        // 横屏显示，且上下翻转，RGB像素格式
        TFT_Ptr.X_Offset = 20;
        TFT_Ptr.Y_Offset = 0;
        TFT_Ptr.Width    = LCD_Height;
        TFT_Ptr.Height   = LCD_Width;
    } else if (direction == Direction_V_Flip) {
        TFT_SEND_CMD(0x36);
        TFT_SEND_DATA(0xC0);        // 竖屏显示，且上下翻转，RGB像素格式
        TFT_Ptr.X_Offset = 0;
        TFT_Ptr.Y_Offset = 20;
        TFT_Ptr.Width    = LCD_Width;
        TFT_Ptr.Height   = LCD_Height;
    }
}

/****************************************************************************************************************************************
*   函 数 名: LCD_Clear
*   函数功能: 清屏函数，将整个LCD设为 LCD.BkgColor 背景色
*   说    明: 先用 LCD_SetBackColor() 设定好背景色，再调用此函数清屏
*****************************************************************************************************************************************/

void LCD_Clear(void)
{
    LCD_SetAddress(0, 0, TFT_Ptr.Width - 1, TFT_Ptr.Height - 1);  // 设置区域

    TFT_DC_D;     // 拉高DC引脚：数据传输
    LCD_CSL;      // 拉低cs引脚，开始传输
    // 修改为16位数据宽度，写入数据更有效率，不需要拆分
    LCD_SPI.Init.DataSize = SPI_DATASIZE_16BIT;   // 16位数据宽度
    HAL_SPI_Init(&LCD_SPI);

    LCD_SPI_Transmit(&LCD_SPI, TFT_Ptr.BkgColor,
                     TFT_Ptr.Width * TFT_Ptr.Height);   // 批量传输
    LCD_CSH;      // 拉高cs引脚，传输结束
    // 改回8位数据宽度，因为指令和参数数据都是按8位传输的
    LCD_SPI.Init.DataSize = SPI_DATASIZE_8BIT;    // 8位数据宽度
    HAL_SPI_Init(&LCD_SPI);
}

/****************************************************************************************************************************************
*   函 数 名: LCD_ClearRect
*   入口参数: x - 起始水平坐标, y - 起始垂直坐标, width - 要清除的横向长度, height - 要清除的纵向长度
*   函数功能: 局部清屏，将指定位置对应区域清除为 LCD.BkgColor 背景色
*****************************************************************************************************************************************/

void LCD_ClearRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    LCD_SetAddress(x, y, x + width - 1, y + height - 1);   // 设置区域

    TFT_DC_D;     // 拉高DC引脚：数据传输
    LCD_CSL;      // 拉低CS引脚，开始传输
    LCD_SPI.Init.DataSize = SPI_DATASIZE_16BIT;
    HAL_SPI_Init(&LCD_SPI);

    LCD_SPI_Transmit(&LCD_SPI, TFT_Ptr.BkgColor, width * height);   // 批量传输
    LCD_CSH;
    LCD_SPI.Init.DataSize = SPI_DATASIZE_8BIT;
    HAL_SPI_Init(&LCD_SPI);
}

/****************************************************************************************************************************************
*   函 数 名: LCD_DrawPoint
*   入口参数: x - 起始水平坐标, y - 起始垂直坐标, color - 要绘制的颜色（24位 RGB888 格式）
*   函数功能: 在指定位置画指定颜色的点
*****************************************************************************************************************************************/

void LCD_DrawPoint(uint16_t x, uint16_t y, uint32_t color)
{
    LCD_SetAddress(x, y, x, y);    // 设置坐标

    TFT_SEND_DATA_16b(color);
}

/****************************************************************************************************************************************
*   函 数 名: LCD_SetAsciiFont
*   入口参数: *fonts - 要设置的ASCII字体
*   函数功能: 设置ASCII字体，可选使用 3216/2412/2010/1608/1206 五种大小字体
*****************************************************************************************************************************************/

void LCD_SetAsciiFont(pFONT *fonts)
{
    LCD_AsciiFonts = fonts;
}

/****************************************************************************************************************************************
*   函 数 名: LCD_DisplayChar
*   入口参数: x - 起始水平坐标, y - 起始垂直坐标, c - ASCII字符
*   函数功能: 在指定位置显示指定的字符
*   说    明: 1. 如需设置字体，使用 LCD_SetAsciiFont(&ASCII_Font24) 设置为 2412号ASCII字体
*             2. 如需设置颜色，使用 LCD_SetColor(0xff0000FF) 设置为蓝色
*             3. 需设置对应的背景色，使用 LCD_SetBackColor(0x000000) 设置为黑色背景色
*****************************************************************************************************************************************/

void LCD_DisplayChar(uint16_t x, uint16_t y, uint8_t c)
{
    uint16_t  index = 0, counter = 0, i = 0, w = 0;
    uint8_t   disChar;     // 存储字符的地址

    c = c - 32;     // 计算ASCII字符的偏移

    for (index = 0; index < LCD_AsciiFonts->Sizes; index++) {
        disChar = LCD_AsciiFonts->pTable[c * LCD_AsciiFonts->Sizes + index]; // 读取字符点模值
        for (counter = 0; counter < 8; counter++) {
            if (disChar & 0x01) {
                LCD_Buff[i] = TFT_Ptr.Color;      // 当前模值为1时，使用画笔颜色画点
            } else {
                LCD_Buff[i] = TFT_Ptr.BkgColor;   // 否则使用背景颜色画点
            }
            disChar >>= 1;
            i++;
            w++;
            if (w == LCD_AsciiFonts->Width) { // 当本行数据达到字符宽度，退出当前循环
                w = 0;                         // 准备下一行的绘制
                break;
            }
        }
    }
    LCD_SetAddress(x, y, x + LCD_AsciiFonts->Width - 1,
                   y + LCD_AsciiFonts->Height - 1);     // 设置坐标
    LCD_WriteBuff(LCD_Buff, LCD_AsciiFonts->Width * LCD_AsciiFonts->Height);  // 写入显存
}

/****************************************************************************************************************************************
*   函 数 名: LCD_DisplayString
*   入口参数: x - 起始水平坐标, y - 起始垂直坐标, p - ASCII字符串的首地址
*   函数功能: 在指定位置显示指定的字符串
*****************************************************************************************************************************************/

void LCD_DisplayString(uint16_t x, uint16_t y, char *p)
{
    while ((x < TFT_Ptr.Width) && (*p != 0)) {  // 判断显示坐标是否超出显示区域、字符串是否为空字符
        LCD_DisplayChar(x, y, *p);
        x += LCD_AsciiFonts->Width; // 显示下一个字符
        p++;                        // 取下一个字符首地址
    }
}

/****************************************************************************************************************************************
*   函 数 名: LCD_SetTextFont
*   入口参数: *fonts - 要设置的文本字体
*   函数功能: 设置文本字体，包括中文和ASCII字符
*   说    明: 1. 可选 3232/2424/2020/1616/1212 五种大小中文字体
*             2. 字库使用小字库，要使用对应函数去取模
*             3. 使用示例 LCD_SetTextFont(&CH_Font24) 设置 2424号中文字体及2412号ASCII字符字体
*****************************************************************************************************************************************/

void LCD_SetTextFont(pFONT *fonts)
{
    LCD_CHFonts = fonts;        // 设置中文字体
    switch (fonts->Width) {
        case 12: LCD_AsciiFonts = &ASCII_Font12;    break;  // 设置ASCII字符字体为 1206
        case 16: LCD_AsciiFonts = &ASCII_Font16;    break;
        case 20: LCD_AsciiFonts = &ASCII_Font20;    break;
        case 24: LCD_AsciiFonts = &ASCII_Font24;    break;
        case 32: LCD_AsciiFonts = &ASCII_Font32;    break;
        default: break;
    }
}

/******************************************************************************************************************************************
*   函 数 名: LCD_DisplayChinese
*   入口参数: x - 起始水平坐标, y - 起始垂直坐标, pText - 中文字符
*   函数功能: 在指定位置显示指定的单个中文字符
*   说    明: 1. 应先使用 LCD_SetTextFont(&CH_Font24) 设置中文字体
*             2. 应先使用 LCD_SetColor(0x0000FF) 设置画笔颜色
*             3. 应先使用 LCD_SetBackColor(0x000000) 设置背景色
*****************************************************************************************************************************************/

void LCD_DisplayChinese(uint16_t x, uint16_t y, char *pText)
{
    uint16_t  i = 0, index = 0, counter = 0;
    uint16_t  addr;           // 点模地址
    uint8_t   disChar;        // 点模值
    uint16_t  Xaddress = 0;   // 水平坐标

    while (1) {
        // 遍历所有的汉字编码，以定位到汉字点模的地址
        if (*(LCD_CHFonts->pTable + (i + 1) * LCD_CHFonts->Sizes + 0) == *pText
            && *(LCD_CHFonts->pTable + (i + 1) * LCD_CHFonts->Sizes + 1) == *(pText + 1)) {
            addr = i;   // 点模地址偏移
            break;
        }
        i += 2;     // 每个中文字符编码占用两字节

        if (i >= LCD_CHFonts->Table_Rows) break;  // 点模列表无对应的汉字
    }
    i = 0;
    for (index = 0; index < LCD_CHFonts->Sizes; index++) {
        disChar = *(LCD_CHFonts->pTable + (addr) * LCD_CHFonts->Sizes + index); // 读取对应汉字点模地址

        for (counter = 0; counter < 8; counter++) {
            if (disChar & 0x01) {
                LCD_Buff[i] = TFT_Ptr.Color;      // 当前模值为1时，使用画笔颜色画点
            } else {
                LCD_Buff[i] = TFT_Ptr.BkgColor;   // 否则使用背景颜色画点
            }
            i++;
            disChar >>= 1;
            Xaddress++;  // 水平坐标自增

            if (Xaddress == LCD_CHFonts->Width) {   // 如果水平坐标达到字符宽度，退出当前循环
                Xaddress = 0;                        // 准备下一行的绘制
                break;
            }
        }
    }
    LCD_SetAddress(x, y, x + LCD_CHFonts->Width - 1,
                   y + LCD_CHFonts->Height - 1);     // 设置坐标
    LCD_WriteBuff(LCD_Buff, LCD_CHFonts->Width * LCD_CHFonts->Height);  // 写入显存
}

/*****************************************************************************************************************************************
*   函 数 名: LCD_DisplayText
*   入口参数: x - 起始水平坐标, y - 起始垂直坐标, pText - 字符串（可含中文和ASCII字符）
*   函数功能: 在指定位置显示指定的字符串
****************************************************************************************************************************************/

void LCD_DisplayText(uint16_t x, uint16_t y, char *pText)
{
    while (*pText != 0) {   // 判断是否为空字符
        if (*pText <= 0x7F) {                   // 判断是否为ASCII字符
            LCD_DisplayChar(x, y, *pText);      // 显示ASCII
            x += LCD_AsciiFonts->Width;         // 水平坐标增加一个字符宽
            pText++;                            // 字符串首地址+1
        } else {                                // 非ASCII字符，即为汉字
            LCD_DisplayChinese(x, y, pText);    // 显示汉字
            x += LCD_CHFonts->Width;            // 水平坐标增加一个字符宽
            pText += 2;                         // 字符串首地址+2，汉字的编码要2字节
        }
    }
}

/*****************************************************************************************************************************************
*   函 数 名: LCD_DisplayNumber
*   入口参数: x - 起始水平坐标, y - 起始垂直坐标, number - 要显示的整数(-2147483648~2147483647)
*             len - 数字的位数，当 len 大于实际长度时在前面补空格
*   函数功能: 在指定位置显示指定的整数
*****************************************************************************************************************************************/

void LCD_DisplayNumber(uint16_t x, uint16_t y, int32_t number, uint8_t len)
{
    char Number_Buffer[15];              // 用于存储转换后的字符串

    sprintf(Number_Buffer, "%0*ld", len, (long)number);  // 将 number 转换成字符串并指定显示长度

    LCD_DisplayString(x, y, (char *)Number_Buffer); // 将转换得到的字符串显示出来
}

/***************************************************************************************************************************************
*   函 数 名: LCD_DisplayDecimals
*   入口参数: x - 起始水平坐标, y - 起始垂直坐标, decimals - 要显示的小数
*             len - 整个数总长度（含小数点和负号）
*             decs - 要保留的小数位数
*   函数功能: 在指定位置显示指定的浮点小数
*****************************************************************************************************************************************/

void LCD_DisplayDecimals(uint16_t x, uint16_t y, double decimals,
                          uint8_t len, uint8_t decs)
{
    char Number_Buffer[20];

    sprintf(Number_Buffer, "%0*.*lf", len, decs, decimals);

    LCD_DisplayString(x, y, (char *)Number_Buffer);
}

/***************************************************************************************************************************************
*   函 数 名: LCD_DrawLine
*   入口参数: x1 - 起点水平坐标, y1 - 起点垂直坐标, x2 - 终点水平坐标, y2 - 终点垂直坐标
*   函数功能: 两点之间画线
*   说    明: 该函数移植于ST官方样例程序
*****************************************************************************************************************************************/

#define ABS(X)  ((X) > 0 ? (X) : -(X))

void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    int16_t deltax = 0, deltay = 0, x = 0, y = 0, xinc1 = 0, xinc2 = 0,
            yinc1 = 0, yinc2 = 0, den = 0, num = 0, numadd = 0,
            numpixels = 0, curpixel = 0;

    deltax = ABS(x2 - x1);
    deltay = ABS(y2 - y1);
    x = x1;  y = y1;

    if (x2 >= x1) { xinc1 = 1;  xinc2 = 1; }
    else          { xinc1 = -1; xinc2 = -1; }

    if (y2 >= y1) { yinc1 = 1;  yinc2 = 1; }
    else          { yinc1 = -1; yinc2 = -1; }

    if (deltax >= deltay) {
        xinc1 = 0;  yinc2 = 0;
        den = deltax;  num = deltax / 2;
        numadd = deltay;  numpixels = deltax;
    } else {
        xinc2 = 0;  yinc1 = 0;
        den = deltay;  num = deltay / 2;
        numadd = deltax;  numpixels = deltay;
    }

    for (curpixel = 0; curpixel <= numpixels; curpixel++) {
        LCD_DrawPoint(x, y, TFT_Ptr.Color);
        num += numadd;
        if (num >= den) { num -= den;  x += xinc1;  y += yinc1; }
        x += xinc2;  y += yinc2;
    }
}

/***************************************************************************************************************************************
*   函 数 名: LCD_DrawLine_V
*   入口参数: x - 水平坐标, y - 垂直坐标, height - 垂直长度
*   函数功能: 在指定位置画指定长度的 垂直 线
*   说    明: 如果只是画垂直线条，使用此函数速度比 LCD_DrawLine 快很多
*****************************************************************************************************************************************/

void LCD_DrawLine_V(uint16_t x, uint16_t y, uint16_t height)
{
    uint16_t i;

    for (i = 0; i < height; i++) {
        LCD_Buff[i] = TFT_Ptr.Color;  // 写入缓冲区
    }
    LCD_SetAddress(x, y, x, y + height - 1);       // 设置坐标

    LCD_WriteBuff(LCD_Buff, height);               // 写入显存
}

/****************************************************************************
*   函 数 名: LCD_DrawLine_H
*   入口参数: x - 水平坐标, y - 垂直坐标, width - 水平长度
*   函数功能: 在指定位置画指定长度的 水平 线
******************************************/

void LCD_DrawLine_H(uint16_t x, uint16_t y, uint16_t width)
{
    uint16_t i;

    for (i = 0; i < width; i++) {
        LCD_Buff[i] = TFT_Ptr.Color;
    }
    LCD_SetAddress(x, y, x + width - 1, y);

    LCD_WriteBuff(LCD_Buff, width);
}

/***************************************************************************************************************************************
*   函 数 名: LCD_DrawRect
*   入口参数: x - 水平坐标, y - 垂直坐标, width - 水平长度, height - 垂直长度
*   函数功能: 在指定位置画指定大小的矩形边框
*****************************************************************************************************************************************/

void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    // 绘制水平线
    LCD_DrawLine_H(x, y, width);
    LCD_DrawLine_H(x, y + height - 1, width);

    // 绘制垂直线
    LCD_DrawLine_V(x, y, height);
    LCD_DrawLine_V(x + width - 1, y, height);
}

/***************************************************************************************************************************************
*   函 数 名: LCD_DrawCircle
*   入口参数: x - 圆心水平坐标, y - 圆心垂直坐标, r - 半径
*   函数功能: 以 (x,y) 绘制半径为 r 的圆边框
*****************************************************************************************************************************************/

void LCD_DrawCircle(uint16_t x, uint16_t y, uint16_t r)
{
    int Xadd = -r, Yadd = 0, err = 2 - 2 * r, e2;
    do {
        LCD_DrawPoint(x - Xadd, y + Yadd, TFT_Ptr.Color);
        LCD_DrawPoint(x + Xadd, y + Yadd, TFT_Ptr.Color);
        LCD_DrawPoint(x + Xadd, y - Yadd, TFT_Ptr.Color);
        LCD_DrawPoint(x - Xadd, y - Yadd, TFT_Ptr.Color);

        e2 = err;
        if (e2 <= Yadd) { err += ++Yadd * 2 + 1; if (-Xadd == Yadd && e2 <= Xadd) e2 = 0; }
        if (e2 > Xadd) err += ++Xadd * 2 + 1;
    } while (Xadd <= 0);
}

/***************************************************************************************************************************************
*   函 数 名: LCD_DrawEllipse
*   入口参数: x - 圆心水平坐标, y - 圆心垂直坐标, r1 - 水平轴长, r2 - 垂直轴长
*   函数功能: 以 (x,y) 绘制水平轴为 r1、垂直轴为 r2 的椭圆边框
*****************************************************************************************************************************************/

void LCD_DrawEllipse(int x, int y, int r1, int r2)
{
    int Xadd = -r1, Yadd = 0, err = 2 - 2 * r1, e2;
    float K = 0, rad1 = r1, rad2 = r2;

    if (r1 > r2) {
        do {
            K = rad1 / rad2;
            LCD_DrawPoint(x - Xadd, y + (int)(Yadd / K), TFT_Ptr.Color);
            LCD_DrawPoint(x + Xadd, y + (int)(Yadd / K), TFT_Ptr.Color);
            LCD_DrawPoint(x + Xadd, y - (int)(Yadd / K), TFT_Ptr.Color);
            LCD_DrawPoint(x - Xadd, y - (int)(Yadd / K), TFT_Ptr.Color);
            e2 = err;
            if (e2 <= Yadd) { err += ++Yadd * 2 + 1; if (-Xadd == Yadd && e2 <= Xadd) e2 = 0; }
            if (e2 > Xadd) err += ++Xadd * 2 + 1;
        } while (Xadd <= 0);
    } else {
        Yadd = -r2;  Xadd = 0;
        do {
            K = rad2 / rad1;
            LCD_DrawPoint(x - (int)(Xadd / K), y + Yadd, TFT_Ptr.Color);
            LCD_DrawPoint(x + (int)(Xadd / K), y + Yadd, TFT_Ptr.Color);
            LCD_DrawPoint(x + (int)(Xadd / K), y - Yadd, TFT_Ptr.Color);
            LCD_DrawPoint(x - (int)(Xadd / K), y - Yadd, TFT_Ptr.Color);
            e2 = err;
            if (e2 <= Xadd) { err += ++Xadd * 3 + 1; if (-Yadd == Xadd && e2 <= Yadd) e2 = 0; }
            if (e2 > Yadd) err += ++Yadd * 3 + 1;
        } while (Yadd <= 0);
    }
}

/***************************************************************************************************************************************
*   函 数 名: LCD_FillRect
*   入口参数: x - 水平坐标, y - 垂直坐标, width - 水平长度, height - 垂直长度
*   函数功能: 绘制以 (x,y) 为起点的指定大小实心矩形
*****************************************************************************************************************************************/

void LCD_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    LCD_SetAddress(x, y, x + width - 1, y + height - 1);  // 设置坐标

    TFT_DC_D;     // 拉高DC引脚：数据传输
    LCD_CSL;      // 拉低CS引脚，开始传输
    LCD_SPI.Init.DataSize = SPI_DATASIZE_16BIT;
    HAL_SPI_Init(&LCD_SPI);

    LCD_SPI_Transmit(&LCD_SPI, TFT_Ptr.Color, width * height);
    LCD_CSH;
    LCD_SPI.Init.DataSize = SPI_DATASIZE_8BIT;
    HAL_SPI_Init(&LCD_SPI);
}

/***************************************************************************************************************************************
*   函 数 名: LCD_FillCircle
*   入口参数: x - 圆心水平坐标, y - 圆心垂直坐标, r - 半径
*   函数功能: 以 (x,y) 绘制半径为 r 的实心圆
*****************************************************************************************************************************************/

void LCD_FillCircle(uint16_t x, uint16_t y, uint16_t r)
{
    int32_t  D;
    uint32_t CurX, CurY;

    D = 3 - (r << 1);
    CurX = 0;  CurY = r;

    while (CurX <= CurY) {
        if (CurY > 0) {
            LCD_DrawLine_V(x - CurX, y - CurY, 2 * CurY);
            LCD_DrawLine_V(x + CurX, y - CurY, 2 * CurY);
        }
        if (CurX > 0) {
            LCD_DrawLine_V(x - CurY, y - CurX, 2 * CurX);
            LCD_DrawLine_V(x + CurY, y - CurX, 2 * CurX);
        }
        if (D < 0) {
            D += (CurX << 2) + 6;
        } else {
            D += ((CurX - CurY) << 2) + 10;
            CurY--;
        }
        CurX++;
    }
    LCD_DrawCircle(x, y, r);
}

/***************************************************************************************************************************************
*   函 数 名: LCD_DrawImage
*   入口参数: x - 起始水平坐标, y - 起始垂直坐标, width - 图片的水平长度, height - 图片的垂直长度
*            *pImage - 图片数据存储区首地址
*   函数功能: 在指定坐标处显示图片
*   说    明: 1.要显示的图片需要先进行取模，并清楚图片的长度和宽度
*             2.使用 LCD_SetColor() 函数设置画笔颜色、LCD_SetBackColor() 设置背景颜色
*****************************************************************************************************************************************/

void LCD_DrawImage(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                   const uint8_t *pImage)
{
    uint8_t   disChar;
    uint16_t  Xaddress = x;
    uint16_t  Yaddress = y;
    uint16_t  i = 0, j = 0, m = 0;
    uint16_t  BuffCount = 0;
    uint16_t  Buff_Height;       // 缓冲区能容纳的行数

    // 因为缓冲区大小有限，需要分段写入
    Buff_Height = (sizeof(LCD_Buff) / 2) / height;  // 计算缓冲区能写入图片的多少行

    for (i = 0; i < height; i++) {          // 循环逐行写入
        for (j = 0; j < (float)width / 8; j++) {
            disChar = *pImage;

            for (m = 0; m < 8; m++) {
                if (disChar & 0x01) {
                    LCD_Buff[BuffCount] = TFT_Ptr.Color;
                } else {
                    LCD_Buff[BuffCount] = TFT_Ptr.BkgColor;
                }
                disChar >>= 1;    // 模值移位
                Xaddress++;       // 水平坐标自增
                BuffCount++;      // 缓冲区计数
                if ((Xaddress - x) == width) {   // 如果达到行宽，准备下一行
                    Xaddress = x;
                    break;
                }
            }
            pImage++;
        }
        if (BuffCount == Buff_Height * width) {  // 达到缓冲区可容纳的上限
            BuffCount = 0;

            LCD_SetAddress(x, Yaddress, x + width - 1, Yaddress + Buff_Height - 1);
            LCD_WriteBuff(LCD_Buff, width * Buff_Height);

            Yaddress = Yaddress + Buff_Height;   // 地址偏移
        }
        if ((i + 1) == height) { // 最后一行时
            LCD_SetAddress(x, Yaddress, x + width - 1, i + y);
            LCD_WriteBuff(LCD_Buff, width * (i + 1 + y - Yaddress));
        }
    }
}

/***************************************************************************************************************************************
*   函 数 名: LCD_CopyBuffer
*   入口参数: x - 起始水平坐标, y - 起始垂直坐标, width - 目标区域水平长度, height - 目标区域垂直长度
*            *pImage - 数据存储区首地址
*   函数功能: 在指定坐标处直接将数据复制到屏幕显存
*   说    明: 此批量复制函数用于对接LVGL等高级图形库或者摄像头采集图像显示
*****************************************************************************************************************************************/

void LCD_CopyBuffer(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                    uint16_t *DataBuff)
{
    LCD_SetAddress(x, y, x + width - 1, y + height - 1);
    LCD_SPI.Init.DataSize = SPI_DATASIZE_16BIT;
    HAL_SPI_Init(&LCD_SPI);
    TFT_DC_D;     // 拉高DC引脚：数据传输
    LCD_CSL;      // 拉低CS引脚，开始传输

    LCD_SPI_TransmitBuffer(&LCD_SPI, DataBuff, width * height);
    LCD_CSH;
    LCD_SPI.Init.DataSize = SPI_DATASIZE_8BIT;
    HAL_SPI_Init(&LCD_SPI);
}

/******************************
*
* 下面几个函数修改于HAL的库函数，目的是为了SPI传输数据不限数据长度的写入，以减少传输耗时
*
********************************/

/**
  * @brief Handle SPI Communication Timeout.
  */
static HAL_StatusTypeDef LCD_SPI_WaitOnFlagUntilTimeout(SPI_HandleTypeDef *hspi,
                                                         uint32_t Flag, FlagStatus Status,
                                                         uint32_t Tickstart, uint32_t Timeout)
{
    while ((__HAL_SPI_GET_FLAG(hspi, Flag) ? SET : RESET) == Status) {
        if ((((HAL_GetTick() - Tickstart) >= Timeout) && (Timeout != HAL_MAX_DELAY))
            || (Timeout == 0U)) {
            return HAL_TIMEOUT;
        }
    }
    return HAL_OK;
}

/**
  * @brief  Close Transfer and clear flags.
  */
static void LCD_SPI_CloseTransfer(SPI_HandleTypeDef *hspi)
{
    uint32_t itflag = hspi->Instance->SR;

    __HAL_SPI_CLEAR_EOTFLAG(hspi);
    __HAL_SPI_CLEAR_TXTFFLAG(hspi);
    __HAL_SPI_DISABLE(hspi);
    __HAL_SPI_DISABLE_IT(hspi, (SPI_IT_EOT | SPI_IT_TXP | SPI_IT_RXP | SPI_IT_DXP
                                | SPI_IT_UDR | SPI_IT_OVR | SPI_IT_FRE | SPI_IT_MODF));
    CLEAR_BIT(hspi->Instance->CFG1, SPI_CFG1_TXDMAEN | SPI_CFG1_RXDMAEN);

    if (hspi->State != HAL_SPI_STATE_BUSY_RX) {
        if ((itflag & SPI_FLAG_UDR) != 0UL) {
            SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_UDR);
            __HAL_SPI_CLEAR_UDRFLAG(hspi);
        }
    }
    if (hspi->State != HAL_SPI_STATE_BUSY_TX) {
        if ((itflag & SPI_FLAG_OVR) != 0UL) {
            SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_OVR);
            __HAL_SPI_CLEAR_OVRFLAG(hspi);
        }
    }
    if ((itflag & SPI_FLAG_MODF) != 0UL) {
        SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_MODF);
        __HAL_SPI_CLEAR_MODFFLAG(hspi);
    }
    if ((itflag & SPI_FLAG_FRE) != 0UL) {
        SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FRE);
        __HAL_SPI_CLEAR_FREFLAG(hspi);
    }

    hspi->TxXferCount = (uint16_t)0UL;
    hspi->RxXferCount = (uint16_t)0UL;
}

/**
  * @brief  专为屏幕填充而修改，不需要考虑颜色混合
  * @param  hspi   : spi的句柄
  * @param  pData  : 要写入的数据
  * @param  Size   : 数据大小
  * @retval HAL status
  */
static HAL_StatusTypeDef LCD_SPI_Transmit(SPI_HandleTypeDef *hspi,
                                           uint16_t pData, uint32_t Size)
{
    uint32_t    tickstart;
    uint32_t    Timeout = 1000;      // 超时判断
    uint32_t    LCD_pData_32bit;     // 做32位写入时构建
    uint32_t    LCD_TxDataCount;     // 传输计数
    HAL_StatusTypeDef errorcode = HAL_OK;

    assert_param(IS_SPI_DIRECTION_2LINES_OR_1LINE_2LINES_TXONLY(hspi->Init.Direction));
    __HAL_LOCK(hspi);
    tickstart = HAL_GetTick();

    if (hspi->State != HAL_SPI_STATE_READY) {
        errorcode = HAL_BUSY;
        __HAL_UNLOCK(hspi);
        return errorcode;
    }
    if (Size == 0UL) {
        errorcode = HAL_ERROR;
        __HAL_UNLOCK(hspi);
        return errorcode;
    }

    hspi->State       = HAL_SPI_STATE_BUSY_TX;
    hspi->ErrorCode   = HAL_SPI_ERROR_NONE;

    LCD_TxDataCount   = Size;                // 要传输的数据长度
    LCD_pData_32bit   = (pData << 16) | pData;   // 32位写入时合并2个像素的纯色

    hspi->pRxBuffPtr  = NULL;
    hspi->RxXferSize  = (uint16_t)0UL;
    hspi->RxXferCount = (uint16_t)0UL;
    hspi->TxISR       = NULL;
    hspi->RxISR       = NULL;

    if (hspi->Init.Direction == SPI_DIRECTION_1LINE)
        SPI_1LINE_TX(hspi);

    // 不使用硬件 TSIZE 控制，此处赋值为0，不限制传输的数据长度
    MODIFY_REG(hspi->Instance->CR2, SPI_CR2_TSIZE, 0);
    __HAL_SPI_ENABLE(hspi);

    if (hspi->Init.Mode == SPI_MODE_MASTER)
        SET_BIT(hspi->Instance->CR1, SPI_CR1_CSTART);

    while (LCD_TxDataCount > 0UL) {
        if (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_TXP)) {
            if ((hspi->TxXferCount > 1UL)
                && (hspi->Init.FifoThreshold > SPI_FIFO_THRESHOLD_01DATA)) {
                *((__IO uint32_t *)&hspi->Instance->TXDR) = LCD_pData_32bit;
                LCD_TxDataCount -= (uint16_t)2UL;
            } else {
                *((__IO uint16_t *)&hspi->Instance->TXDR) = (uint16_t)pData;
                LCD_TxDataCount--;
            }
        } else {
            if ((((HAL_GetTick() - tickstart) >= Timeout) && (Timeout != HAL_MAX_DELAY))
                || (Timeout == 0U)) {
                LCD_SPI_CloseTransfer(hspi);
                __HAL_UNLOCK(hspi);
                SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_TIMEOUT);
                hspi->State = HAL_SPI_STATE_READY;
                return HAL_ERROR;
            }
        }
    }

    if (LCD_SPI_WaitOnFlagUntilTimeout(hspi, SPI_SR_TXC, RESET, tickstart, Timeout) != HAL_OK)
        SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);

    SET_BIT((hspi)->Instance->CR1, SPI_CR1_CSUSP);
    if (LCD_SPI_WaitOnFlagUntilTimeout(hspi, SPI_FLAG_SUSP, RESET, tickstart, Timeout) != HAL_OK)
        SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);

    LCD_SPI_CloseTransfer(hspi);
    SET_BIT((hspi)->Instance->IFCR, SPI_IFCR_SUSPC);
    __HAL_UNLOCK(hspi);
    hspi->State = HAL_SPI_STATE_READY;

    if (hspi->ErrorCode != HAL_SPI_ERROR_NONE) return HAL_ERROR;
    return errorcode;
}

/**
  * @brief  专为批量写入而修改，使之不限长度的批量传输
  * @param  hspi   : spi的句柄
  * @param  pData  : 要写入的数据
  * @param  Size   : 数据大小
  * @retval HAL status
  */
static HAL_StatusTypeDef LCD_SPI_TransmitBuffer(SPI_HandleTypeDef *hspi,
                                                 uint16_t *pData, uint32_t Size)
{
    uint32_t    tickstart;
    uint32_t    Timeout = 1000;      // 超时判断
    uint32_t    LCD_TxDataCount;     // 传输计数
    HAL_StatusTypeDef errorcode = HAL_OK;

    assert_param(IS_SPI_DIRECTION_2LINES_OR_1LINE_2LINES_TXONLY(hspi->Init.Direction));
    __HAL_LOCK(hspi);
    tickstart = HAL_GetTick();

    if (hspi->State != HAL_SPI_STATE_READY) {
        errorcode = HAL_BUSY;
        __HAL_UNLOCK(hspi);
        return errorcode;
    }
    if (Size == 0UL) {
        errorcode = HAL_ERROR;
        __HAL_UNLOCK(hspi);
        return errorcode;
    }

    hspi->State       = HAL_SPI_STATE_BUSY_TX;
    hspi->ErrorCode   = HAL_SPI_ERROR_NONE;

    LCD_TxDataCount   = Size;

    hspi->pRxBuffPtr  = NULL;
    hspi->RxXferSize  = (uint16_t)0UL;
    hspi->RxXferCount = (uint16_t)0UL;
    hspi->TxISR       = NULL;
    hspi->RxISR       = NULL;

    if (hspi->Init.Direction == SPI_DIRECTION_1LINE)
        SPI_1LINE_TX(hspi);

    MODIFY_REG(hspi->Instance->CR2, SPI_CR2_TSIZE, 0);
    __HAL_SPI_ENABLE(hspi);

    if (hspi->Init.Mode == SPI_MODE_MASTER)
        SET_BIT(hspi->Instance->CR1, SPI_CR1_CSTART);

    while (LCD_TxDataCount > 0UL) {
        if (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_TXP)) {
            if ((LCD_TxDataCount > 1UL)
                && (hspi->Init.FifoThreshold > SPI_FIFO_THRESHOLD_01DATA)) {
                *((__IO uint32_t *)&hspi->Instance->TXDR) = *((uint32_t *)pData);
                pData += 2;
                LCD_TxDataCount -= 2;
            } else {
                *((__IO uint16_t *)&hspi->Instance->TXDR) = *((uint16_t *)pData);
                pData += 1;
                LCD_TxDataCount--;
            }
        } else {
            if ((((HAL_GetTick() - tickstart) >= Timeout) && (Timeout != HAL_MAX_DELAY))
                || (Timeout == 0U)) {
                LCD_SPI_CloseTransfer(hspi);
                __HAL_UNLOCK(hspi);
                SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_TIMEOUT);
                hspi->State = HAL_SPI_STATE_READY;
                return HAL_ERROR;
            }
        }
    }

    if (LCD_SPI_WaitOnFlagUntilTimeout(hspi, SPI_SR_TXC, RESET, tickstart, Timeout) != HAL_OK)
        SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);

    SET_BIT((hspi)->Instance->CR1, SPI_CR1_CSUSP);
    if (LCD_SPI_WaitOnFlagUntilTimeout(hspi, SPI_FLAG_SUSP, RESET, tickstart, Timeout) != HAL_OK)
        SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_FLAG);

    LCD_SPI_CloseTransfer(hspi);
    SET_BIT((hspi)->Instance->IFCR, SPI_IFCR_SUSPC);
    __HAL_UNLOCK(hspi);
    hspi->State = HAL_SPI_STATE_READY;

    if (hspi->ErrorCode != HAL_SPI_ERROR_NONE) return HAL_ERROR;
    return errorcode;
}
