/***
文件说明：
    *
    *  1.SCCB相关函数（同I2C一样时序），使用模拟I2C接口
    *  2.例程默认配置 OV5640 为 4:3(1280*960) 43帧的配置
    *  3.开启了DMA并使能了中断，移植的时候需要移植对应的中断
    *
    ************************************************************************************************************************
***/

#include "main.h"
#include "bsp_cam.h"
#include "bsp_cam_cfg.h"
#include "stm32h7xx_hal_jpeg.h"
#include <stdio.h>

uint16_t    Device_ID;
DCMI_HandleTypeDef   hdcmi;            // DCMI句柄
DMA_HandleTypeDef    DMA_Handle_dcmi;  // DMA句柄
JPEG_HandleTypeDef   hjpeg;            // JPEG硬件编解码句柄

volatile uint8_t  OV5640_FrameState = 0; // DCMI 帧就绪标志
volatile uint8_t  OV5640_FPS;             // 帧率

/* ====================== SCCB（模拟I2C）相关函数 ====================== */

/*****************************************************************************************
*   函 数 名: SCCB_GPIO_Config
*   入口参数: 无
*   返 回 值: 无
*   函数功能: 初始化IIC的GPIO口,推挽输出
*   说    明: 由于IIC通信速度不高，这里的IO口速度配置为2M即可
******************************************************************************************/

void SCCB_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    SCCB_SCL_CLK_ENABLE;    // 初始化IO口时钟
    SCCB_SDA_CLK_ENABLE;

    GPIO_InitStruct.Mode    = GPIO_MODE_OUTPUT_OD;         // 开漏输出
    GPIO_InitStruct.Pull    = GPIO_NOPULL;                 // 不带上下拉
    GPIO_InitStruct.Speed   = GPIO_SPEED_FREQ_LOW;        // 速度等级
    GPIO_InitStruct.Pin     = SCCB_SDA_PIN;                // SDA引脚
    HAL_GPIO_Init(SCCB_SDA_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Mode    = GPIO_MODE_OUTPUT_PP;         // 推挽输出
    GPIO_InitStruct.Pin     = SCCB_SCL_PIN;                // SCL引脚
    HAL_GPIO_Init(SCCB_SCL_PORT, &GPIO_InitStruct);

    HAL_GPIO_WritePin(SCCB_SCL_PORT, SCCB_SCL_PIN, GPIO_PIN_SET);    // SCL输出高电平
    HAL_GPIO_WritePin(SCCB_SDA_PORT, SCCB_SDA_PIN, GPIO_PIN_SET);    // SDA输出高电平
}

/*****************************************************************************************
*   函 数 名: SCCB_Delay
*   入口参数: a - 延时时间
*   返 回 值: 无
*   函数功能: 简单延时函数
*   说    明: 为了移植的简便性且对延时精度要求不高，所以不需要使用定时器做延时
******************************************************************************************/

void SCCB_Delay(uint32_t a)
{
    volatile uint16_t i;
    while (a--) {
        for (i = 0; i < 6; i++);
    }
}

/*****************************************************************************************
*   函 数 名: SCCB_Start
*   入口参数: 无
*   返 回 值: 无
*   函数功能: IIC起始信号
*   说    明: 在SCL处于高电平期间，SDA由高到低跳变为起始信号
******************************************************************************************/

void SCCB_Start(void)
{
    SCCB_SDA(1);
    SCCB_SCL(1);
    SCCB_Delay(SCCB_DelayVaule);

    SCCB_SDA(0);
    SCCB_Delay(SCCB_DelayVaule);
    SCCB_SCL(0);
    SCCB_Delay(SCCB_DelayVaule);
}

/*****************************************************************************************
*   函 数 名: SCCB_Stop
*   入口参数: 无
*   返 回 值: 无
*   函数功能: IIC停止信号
*   说    明: 在SCL处于高电平期间，SDA由低到高跳变为停止信号
******************************************************************************************/

void SCCB_Stop(void)
{
    SCCB_SCL(0);
    SCCB_Delay(SCCB_DelayVaule);
    SCCB_SDA(0);
    SCCB_Delay(SCCB_DelayVaule);

    SCCB_SCL(1);
    SCCB_Delay(SCCB_DelayVaule);
    SCCB_SDA(1);
    SCCB_Delay(SCCB_DelayVaule);
}

/*****************************************************************************************
*   函 数 名: SCCB_ACK
*   入口参数: 无
*   返 回 值: 无
*   函数功能: IIC应答信号
*   说    明: 在SCL为高电平期间，SDA引脚输出为低电平，产生应答信号
******************************************************************************************/

void SCCB_ACK(void)
{
    SCCB_SCL(0);
    SCCB_Delay(SCCB_DelayVaule);
    SCCB_SDA(0);
    SCCB_Delay(SCCB_DelayVaule);
    SCCB_SCL(1);
    SCCB_Delay(SCCB_DelayVaule);

    SCCB_SCL(0);        // SCL输出低时，SDA应立即拉高，释放总线
    SCCB_SDA(1);

    SCCB_Delay(SCCB_DelayVaule);
}

/*****************************************************************************************
*   函 数 名: SCCB_NoACK
*   入口参数: 无
*   返 回 值: 无
*   函数功能: IIC非应答信号
*   说    明: 在SCL为高电平期间，若SDA引脚为高电平，产生非应答信号
******************************************************************************************/

void SCCB_NoACK(void)
{
    SCCB_SCL(0);
    SCCB_Delay(SCCB_DelayVaule);
    SCCB_SDA(1);
    SCCB_Delay(SCCB_DelayVaule);
    SCCB_SCL(1);
    SCCB_Delay(SCCB_DelayVaule);

    SCCB_SCL(0);
    SCCB_Delay(SCCB_DelayVaule);
}

/*****************************************************************************************
*   函 数 名: SCCB_WaitACK
*   入口参数: 无
*   返 回 值: ACK_OK - 应答正常, ACK_ERR - 无应答
*   函数功能: 等待接收设备发出应答信号
*   说    明: 在SCL为高电平期间，若检测到SDA引脚为低电平，则接收设备响应正常
******************************************************************************************/

uint8_t SCCB_WaitACK(void)
{
    SCCB_SDA(1);
    SCCB_Delay(SCCB_DelayVaule);
    SCCB_SCL(1);
    SCCB_Delay(SCCB_DelayVaule);

    if (HAL_GPIO_ReadPin(SCCB_SDA_PORT, SCCB_SDA_PIN) != 0) {  // 判断设备是否有做出响应
        SCCB_SCL(0);
        SCCB_Delay(SCCB_DelayVaule);
        return ACK_ERR;     // 无应答
    } else {
        SCCB_SCL(0);
        SCCB_Delay(SCCB_DelayVaule);
        return ACK_OK;      // 应答正常
    }
}

/*****************************************************************************************
*   函 数 名: SCCB_WriteByte
*   入口参数: IIC_Data - 要写入的8位数据
*   返 回 值: ACK_OK  - 设备响应正常  ACK_ERR - 设备响应错误
*   函数功能: 写一字节数据
*   说    明: 高位在前
******************************************************************************************/

uint8_t SCCB_WriteByte(uint8_t IIC_Data)
{
    uint8_t i;

    for (i = 0; i < 8; i++) {
        SCCB_SDA(IIC_Data & 0x80);

        SCCB_Delay(SCCB_DelayVaule);
        SCCB_SCL(1);
        SCCB_Delay(SCCB_DelayVaule);
        SCCB_SCL(0);
        if (i == 7) {
            SCCB_SDA(1);
        }
        IIC_Data <<= 1;
    }

    return SCCB_WaitACK(); // 等待设备响应
}

/*****************************************************************************************
*   函 数 名: SCCB_ReadByte
*   入口参数: ACK_Mode - 响应模式，输入1则发出应答信号，输入0发出非应答信号
*   返 回 值: 读取到的数据
*   函数功能: 读一字节数据
*   说    明: 1.高位在前    2.应在主机接收最后一字节数据时发送非应答信号
******************************************************************************************/

uint8_t SCCB_ReadByte(uint8_t ACK_Mode)
{
    uint8_t IIC_Data = 0;
    uint8_t i = 0;

    for (i = 0; i < 8; i++) {
        IIC_Data <<= 1;

        SCCB_SCL(1);
        SCCB_Delay(SCCB_DelayVaule);
        IIC_Data |= (HAL_GPIO_ReadPin(SCCB_SDA_PORT, SCCB_SDA_PIN) & 0x01);
        SCCB_SCL(0);
        SCCB_Delay(SCCB_DelayVaule);
    }

    if (ACK_Mode == 1)              // 应答信号
        SCCB_ACK();
    else
        SCCB_NoACK();               // 非应答信号

    return IIC_Data;
}

/*************************************************************************************************************************************
*   函 数 名: SCCB_WriteHandle
*   入口参数: addr - 要进行操作的寄存器(8位地址)
*   返 回 值: SUCCESS - 操作成功，ERROR - 操作失败
*   函数功能: 对指定的寄存器(8位地址)执行写操作，OV2640用到
************************************************************************************************************************************/

uint8_t SCCB_WriteHandle(uint8_t addr)
{
    uint8_t status;     // 状态标志位

    SCCB_Start();       // 启动IIC通信
    if (SCCB_WriteByte(OV2640_DEVICE_ADDRESS) == ACK_OK) { // 写数据指令
        if (SCCB_WriteByte((uint8_t)(addr)) != ACK_OK) {
            status = OV5640_Error;    // 操作失败
            return status;
        }
    }
    status = OV5640_Success;   // 操作成功
    return status;
}

/*************************************************************************************************************************************
*   函 数 名: SCCB_WriteReg
*   入口参数: addr - 要写入的寄存器(8位地址)，value - 要写入的数据
*   返 回 值: SUCCESS - 操作成功，ERROR - 操作失败
*   函数功能: 对指定的寄存器(8位地址)写一字节数据，OV2640用到
************************************************************************************************************************************/

uint8_t SCCB_WriteReg(uint8_t addr, uint8_t value)
{
    uint8_t status;

    SCCB_Start(); // 启动IIC通讯

    if (SCCB_WriteHandle(addr) == OV5640_Success) {  // 写入要操作的寄存器
        if (SCCB_WriteByte(value) != ACK_OK) {        // 写数据
            status = OV5640_Error;
            SCCB_Stop();
            return status;
        }
    }
    SCCB_Stop(); // 停止通讯

    status = OV5640_Success;   // 写入成功
    return status;
}

/*************************************************************************************************************************************
*   函 数 名: SCCB_ReadReg
*   入口参数: addr - 要读取的寄存器(8位地址)
*   返 回 值: 读到的数据
*   函数功能: 对指定的寄存器(8位地址)读取一字节数据，OV2640用到
************************************************************************************************************************************/

uint8_t SCCB_ReadReg(uint8_t addr)
{
    uint8_t value = 0;

    SCCB_Start();       // 启动IIC通信

    if (SCCB_WriteHandle(addr) == OV5640_Success) { // 写入要操作的寄存器
        SCCB_Stop();    // 停止IIC通信
        SCCB_Start();   // 重新启动IIC通讯

        if (SCCB_WriteByte(OV2640_DEVICE_ADDRESS | 0X01) == ACK_OK) { // 发送读命令
            value = SCCB_ReadByte(0);    // 读到最后一个数据时发送非应答信号
        }
        SCCB_Stop();    // 停止IIC通信
    }

    return value;
}

/*************************************************************************************************************************************
*   函 数 名: SCCB_WriteHandle_16Bit
*   入口参数: addr - 要进行操作的寄存器(16位地址)
*   返 回 值: SUCCESS - 操作成功，ERROR - 操作失败
*   函数功能: 对指定的寄存器(16位地址)执行写操作，OV5640用到
************************************************************************************************************************************/

uint8_t SCCB_WriteHandle_16Bit(uint16_t addr)
{
    uint8_t status;     // 状态标志位

    SCCB_Start();       // 启动IIC通信
    if (SCCB_WriteByte(OV5640_DEVICE_ADDRESS) == ACK_OK) { // 写数据指令
        if (SCCB_WriteByte((uint8_t)(addr >> 8)) == ACK_OK) { // 写入16位地址
            if (SCCB_WriteByte((uint8_t)(addr)) != ACK_OK) {
                status = OV5640_Error;    // 操作失败
                return status;
            }
        }
    }
    status = OV5640_Success;   // 操作成功
    return status;
}

/*************************************************************************************************************************************
*   函 数 名: SCCB_WriteReg_16Bit
*   入口参数: addr - 要写入的寄存器(16位地址)  value - 要写入的数据
*   返 回 值: SUCCESS - 操作成功，ERROR - 操作失败
*   函数功能: 对指定的寄存器(16位地址)写一字节数据，OV5640用到
************************************************************************************************************************************/

uint8_t SCCB_WriteReg_16Bit(uint16_t addr, uint8_t value)
{
    uint8_t status;

    SCCB_Start(); // 启动IIC通讯

    if (SCCB_WriteHandle_16Bit(addr) == OV5640_Success) { // 写入要操作的寄存器
        if (SCCB_WriteByte(value) != ACK_OK) {             // 写数据
            status = OV5640_Error;
            SCCB_Stop();
            return status;
        }
    }
    SCCB_Stop(); // 停止通讯

    status = OV5640_Success;   // 写入成功
    return status;
}

/*************************************************************************************************************************************
*   函 数 名: SCCB_ReadReg_16Bit
*   入口参数: addr - 要读取的寄存器(16位地址)
*   返 回 值: 读到的数据
*   函数功能: 对指定的寄存器(16位地址)读取一字节数据，OV5640用到
************************************************************************************************************************************/

uint8_t SCCB_ReadReg_16Bit(uint16_t addr)
{
    uint8_t value = 0;

    SCCB_Start();       // 启动IIC通信

    if (SCCB_WriteHandle_16Bit(addr) == OV5640_Success) { // 写入要操作的寄存器
        SCCB_Stop();    // 停止IIC通信
        SCCB_Start();   // 重新启动IIC通讯

        if (SCCB_WriteByte(OV5640_DEVICE_ADDRESS | 0X01) == ACK_OK) { // 发送读命令
            value = SCCB_ReadByte(0);    // 读到最后一个数据时发送非应答信号
        }
        SCCB_Stop();    // 停止IIC通信
    }

    return value;
}

/*************************************************************************************************************************************
*   函 数 名: SCCB_WriteBuffer_16Bit
*   入口参数: addr - 要写入的寄存器(16位地址)  *pData - 数据区   size - 要传输数据的大小
*   返 回 值: SUCCESS - 操作成功，ERROR - 操作失败
*   函数功能: 对指定的寄存器(16位地址)批量写数据，OV5640 写入自动对焦固件时用到
************************************************************************************************************************************/

uint8_t SCCB_WriteBuffer_16Bit(uint16_t addr, uint8_t *pData, uint32_t size)
{
    uint8_t  status;
    uint32_t i;

    SCCB_Start(); // 启动IIC通讯

    if (SCCB_WriteHandle_16Bit(addr) == OV5640_Success) { // 写入要操作的寄存器
        for (i = 0; i < size; i++) {
            SCCB_WriteByte(*pData); // 写数据
            pData++;
        }
    }
    SCCB_Stop(); // 停止通讯

    status = OV5640_Success;   // 写入成功
    return status;
}

/* ====================== DCMI / DMA / OV5640 相关函数 ====================== */

/*************************************************
*   函 数 名: HAL_DCMI_MspInit
*   入口参数: hdcmi - DCMI_HandleTypeDef定义的变量，即表示定义的 DCMI 句柄
*   函数功能: 初始化 DCMI 引脚
************************************************************************/
void HAL_DCMI_MspInit(DCMI_HandleTypeDef* hdcmi)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hdcmi->Instance == DCMI) {
        __HAL_RCC_DCMI_CLK_ENABLE();    // 使能 DCMI 外设时钟

        __HAL_RCC_GPIOE_CLK_ENABLE();   // 使能相应的GPIO时钟
        __HAL_RCC_GPIOD_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /****************************************************************************
           数据引脚                       时钟和同步引脚
            PC6     ------> DCMI_D0        PB7     ------> DCMI_VSYNC
            PC7     ------> DCMI_D1         PA4     ------> DCMI_HSYNC
            PE0     ------> DCMI_D2        PA6      ------> DCMI_PIXCLK
            PE1     ------> DCMI_D3
            PE4     ------> DCMI_D4        SCCB 控制引脚，初始化在 SCCB 部分
            PD3     ------> DCMI_D5         PB6  ------> SCCB_SCL
            PE5     ------> DCMI_D6        PB9  ------> SCCB_SDA
            PE6     ------> DCMI_D7

           掉电控制引脚
           PD10   ------> PWDN
        ******************************************************************************/

        GPIO_InitStruct.Pin    = GPIO_PIN_1 | GPIO_PIN_0 | GPIO_PIN_5 | GPIO_PIN_4
                               | GPIO_PIN_6;
        GPIO_InitStruct.Mode   = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull   = GPIO_NOPULL;
        GPIO_InitStruct.Speed  = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
        HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

        GPIO_InitStruct.Pin    = GPIO_PIN_3;
        GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
        HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

        GPIO_InitStruct.Pin    = GPIO_PIN_7;
        GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        GPIO_InitStruct.Pin    = GPIO_PIN_7 | GPIO_PIN_6;
        GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        GPIO_InitStruct.Pin    = GPIO_PIN_6 | GPIO_PIN_4;
        GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        // 初始化 PWDN 引脚
        //OV5640_PWDN_ON; // 高电平，进入掉电模式，摄像头停止工作，此时功耗降到最低

        GPIO_InitStruct.Pin    = GPIO_PIN_10;              // PWDN 引脚
        GPIO_InitStruct.Mode   = GPIO_MODE_OUTPUT_PP;      // 推挽输出模式
        GPIO_InitStruct.Pull   = GPIO_PULLUP;              // 上拉
        GPIO_InitStruct.Speed  = GPIO_SPEED_FREQ_LOW;      // 速度等级低
        HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);            // 初始化

        GPIO_InitStruct.Pin    = GPIO_PIN_13;              // DCMI RST
        HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

        DCMI_RST_L;
        OV5640_PWDN_ON;
    }
}

/***************************************************************************************************************************************
*   函 数 名: MX_DCMI_Init
*   函数功能: 配置DCMI相关参数
*   说    明: 8位数据模式，全数据、全帧捕捉，开启中断
*****************************************************************************************************************************************/
void MX_DCMI_Init(void)
{
    hdcmi.Instance                = DCMI;
    hdcmi.Init.SynchroMode        = DCMI_SYNCHRO_HARDWARE;      // 硬件同步模式
    hdcmi.Init.PCKPolarity        = DCMI_PCKPOLARITY_RISING;    // 像素时钟上升沿有效
    hdcmi.Init.VSPolarity         = DCMI_VSPOLARITY_LOW;        // VS低电平有效
    hdcmi.Init.HSPolarity         = DCMI_HSPOLARITY_LOW;        // HS低电平有效
    hdcmi.Init.CaptureRate        = DCMI_CR_ALL_FRAME;          // 每一帧都进行捕获
    hdcmi.Init.ExtendedDataMode   = DCMI_EXTEND_DATA_8B;        // 8位数据模式
    hdcmi.Init.JPEGMode           = DCMI_JPEG_DISABLE;          // 不使用DCMI的JPEG模式
    hdcmi.Init.ByteSelectMode     = DCMI_BSM_ALL;               // DCMI接口捕捉所有数据
    hdcmi.Init.ByteSelectStart    = DCMI_OEBS_ODD;              // 从帧/行的第一个数据开始捕获
    hdcmi.Init.LineSelectMode     = DCMI_LSM_ALL;               // 捕获所有行
    hdcmi.Init.LineSelectStart    = DCMI_OELS_ODD;              // 在帧开始后捕获第一行
    HAL_DCMI_Init(&hdcmi);

    HAL_NVIC_SetPriority(DCMI_IRQn, 0, 5);     // 设置中断优先级
    HAL_NVIC_EnableIRQ(DCMI_IRQn);              // 开启DCMI中断
}

/***************************************************************************************************************************************
*   函 数 名: OV5640_DMA_Init
*   函数功能: 配置 DMA 相关参数
*   说    明: 使用的是DMA2，外设到存储器模式、数据位宽32bit、并开启中断
*****************************************************************************************************************************************/
void OV5640_DMA_Init(void)
{
    __HAL_RCC_DMA2_CLK_ENABLE();   // 使能DMA2时钟

    DMA_Handle_dcmi.Instance                     = DMA2_Stream7;               // DMA2数据流7
    DMA_Handle_dcmi.Init.Request                 = DMA_REQUEST_DCMI;           // DMA请求来自DCMI
    DMA_Handle_dcmi.Init.Direction               = DMA_PERIPH_TO_MEMORY;       // 外设到存储器模式
    DMA_Handle_dcmi.Init.PeriphInc               = DMA_PINC_DISABLE;           // 外设地址禁止自增
    DMA_Handle_dcmi.Init.MemInc                  = DMA_MINC_ENABLE;            // 存储器地址自增
    DMA_Handle_dcmi.Init.PeriphDataAlignment     = DMA_PDATAALIGN_WORD;        // DCMI数据位宽，32位
    DMA_Handle_dcmi.Init.MemDataAlignment        = DMA_MDATAALIGN_WORD;        // 存储器数据位宽，32位
    DMA_Handle_dcmi.Init.Mode                    = DMA_CIRCULAR;               // 循环模式
    DMA_Handle_dcmi.Init.Priority                = DMA_PRIORITY_LOW;           // 优先级低
    DMA_Handle_dcmi.Init.FIFOMode                = DMA_FIFOMODE_ENABLE;        // 使能fifo
    DMA_Handle_dcmi.Init.FIFOThreshold           = DMA_FIFO_THRESHOLD_FULL;    // 全fifo模式，4*32bit大小
    DMA_Handle_dcmi.Init.MemBurst                = DMA_MBURST_SINGLE;          // 单次传输
    DMA_Handle_dcmi.Init.PeriphBurst             = DMA_PBURST_SINGLE;          // 单次传输

    HAL_DMA_Init(&DMA_Handle_dcmi);                        // 配置DMA
    __HAL_LINKDMA(&hdcmi, DMA_Handle, DMA_Handle_dcmi);    // 关联DCMI句柄

    HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 0, 0);         // 设置中断优先级
    HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);                 // 使能中断
}

/***************************************************************************************************************************************
*   函 数 名: DCMI_OV5640_Init
*   函数功能: 初始SCCB、DCMI、DMA以及配置OV5640
*****************************************************************************************************************************************/
int8_t DCMI_OV5640_Init(void)
{
    printf("===== OV5640 Camera Init =====\r\n");
    printf("Sensor: %dx%d, Display: %dx%d, DMA_BufSize: %d\r\n",
           OV5640_Width, OV5640_Height,
           CAM_DISPLAY_Width, CAM_DISPLAY_Height,
           CAM_DMA_BufferSize);

    OV5640_DMA_Init();                       // 初始化DMA配置
    printf("[DMA] DMA2_Stream7 init done\r\n");

    MX_DCMI_Init();                          // 初始化DCMI配置引脚
    printf("[DCMI] DCMI init done\r\n");

    OV5640_PWDN_OFF;  // PWDN 引脚输出低电平，不开启掉电模式
    HAL_Delay(140);
    DCMI_RST_H;
    printf("[PWR] Power-on sequence done\r\n");

    // 复位完成之后，要>=20ms才可执行SCCB配置
    HAL_Delay(40);
    SCCB_GPIO_Config();                      // SCCB引脚初始化
    printf("[SCCB] GPIO config done\r\n");

    HAL_Delay(20);
    SCCB_WriteReg_16Bit(0x3103, 0x11);       // 根据手册的建议，复位之前，直接将时钟输入引脚的时钟作为主时钟
    SCCB_WriteReg_16Bit(0x3008, 0x82);       // 执行一次软复位
    printf("[SCCB] Soft reset done\r\n");

    HAL_Delay(15);                           // 延时>5ms
    Device_ID = OV5640_ReadID();             // 读取器件ID
    printf("[ID] Chip ID: 0x%04X\r\n", Device_ID);

    if (Device_ID == 0x5640) {               // 进行匹配
        printf("[ID] OV5640 OK!\r\n");

        OV5640_Config();                                                  // 配置各项参数
        printf("[CFG] Register config done (%d regs)\r\n",
               (int)(sizeof(OV5640_INIT_Config) / 4));

        OV5640_Set_Framesize(OV5640_Width, OV5640_Height);                // 设置OV5640输出的图像大小
        printf("[FMT] Framesize set: %dx%d\r\n", OV5640_Width, OV5640_Height);

        OV5640_DCMI_Crop(CAM_DISPLAY_Width, CAM_DISPLAY_Height,
                         OV5640_Width, OV5640_Height);                    // 将输出图像裁剪成适应屏幕的大小
        printf("[CROP] Crop to: %dx%d\r\n", CAM_DISPLAY_Width, CAM_DISPLAY_Height);
        printf("===== Camera Init SUCCESS =====\r\n");

        return OV5640_Success;   // 返回成功标志
    } else {
        printf("[ID] OV5640 ERROR! Unexpected ID: 0x%04X\r\n", Device_ID);
        printf("===== Camera Init FAILED =====\r\n");
        return OV5640_Error;     // 返回错误标志
    }
}

/***************************************************************************************************************************************
*   函 数 名: OV5640_DMA_Transmit_Continuous
*   入口参数: DMA_Buffer - DMA将要传输的地址，即用于存储摄像头数据的存储区地址
*             DMA_BufferSize - 传输的数据大小（32位宽）
*   函数功能: 启动DMA传输，连续模式
*   说    明: 1. 开启连续模式之后，会一直进行传输，除非挂起或者停止DCMI
*             2. OV5640使用RGB565模式时，1个像素点需要2个字节来存储
*             3. 因为DMA配置传输数据为32位宽，计算 DMA_BufferSize 时，需要除以4
*                例如：要获取 240*280分辨率图像，需要 240*280*2 = 134400 字节
*                DMA_BufferSize = 134400 / 4 = 33600
*****************************************************************************************************************************************/
void OV5640_DMA_Transmit_Continuous(uint32_t DMA_Buffer, uint32_t DMA_BufferSize)
{
    DMA_Handle_dcmi.Init.Mode = DMA_CIRCULAR;  // 循环模式

    HAL_DMA_Init(&DMA_Handle_dcmi);    // 配置DMA

    // 使能DCMI采集数据,连续采集模式
    HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_CONTINUOUS, DMA_Buffer, DMA_BufferSize);
}

/***************************************************************************************************************************************
*   函 数 名: OV5640_DMA_Transmit_Snapshot
*   入口参数: DMA_Buffer - DMA将要传输的地址
*             DMA_BufferSize - 传输的数据大小（32位宽）
*   函数功能: 启动DMA传输，快照模式，传输一帧图像后停止
*****************************************************************************************************************************************/
void OV5640_DMA_Transmit_Snapshot(uint32_t DMA_Buffer, uint32_t DMA_BufferSize)
{
    DMA_Handle_dcmi.Init.Mode = DMA_NORMAL;  // 正常模式

    HAL_DMA_Init(&DMA_Handle_dcmi);    // 配置DMA

    HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_SNAPSHOT, DMA_Buffer, DMA_BufferSize);
}

/***************************************************************************************************************************************
*   函 数 名: OV5640_DCMI_Suspend
*   函数功能: 挂起DCMI，停止捕获数据
*****************************************************************************************************************************************/
void OV5640_DCMI_Suspend(void)
{
    HAL_DCMI_Suspend(&hdcmi);    // 挂起DCMI
}

/***************************************************************************************************************************************
*   函 数 名: OV5640_DCMI_Resume
*   函数功能: 恢复DCMI，开始捕获数据
*****************************************************************************************************************************************/
void OV5640_DCMI_Resume(void)
{
    (&hdcmi)->State = HAL_DCMI_STATE_BUSY;       // 变更DCMI标志
    (&hdcmi)->Instance->CR |= DCMI_CR_CAPTURE;   // 开启DCMI捕获
}

/***************************************************************************************************************************************
*   函 数 名: OV5640_DCMI_Stop
*   函数功能: 禁止DCMI的DMA请求，停止DCMI捕获，禁止DCMI外设
*****************************************************************************************************************************************/
void OV5640_DCMI_Stop(void)
{
    HAL_DCMI_Stop(&hdcmi);
}

/***************************************************************************************************************************************
*   函 数 名: OV5640_DCMI_Crop
*   入口参数: Displey_XSize 、Displey_YSize - 显示屏幕的尺寸
*             Sensor_XSize、Sensor_YSize - 摄像头实际输出的图像尺寸
*   函数功能: 使用DCMI的裁剪功能，将摄像头输出的图像裁剪成适应屏幕的大小
*   说    明: 1. 因为摄像头和屏幕的分辨率不一定匹配，所以需要裁剪
*             2. 摄像头输出分辨率由 OV5640_Config() 参数决定
*             3. DCMI水平有效像素数的值也需要能被4整除
*             4. 实际可设置水平和垂直偏移，控制用户需要裁剪的区域
*****************************************************************************************************************************************/
int8_t OV5640_DCMI_Crop(uint16_t Displey_XSize, uint16_t Displey_YSize,
                        uint16_t Sensor_XSize, uint16_t Sensor_YSize)
{
    uint16_t DCMI_X_Offset, DCMI_Y_Offset;   // 水平和垂直偏移
    uint16_t DCMI_CAPCNT;                    // 水平有效像素
    uint16_t DCMI_VLINE;                     // 垂直有效行数

    if ((Displey_XSize >= Sensor_XSize) || (Displey_YSize >= Sensor_YSize)) {
        return OV5640_Error;   // 如果实际显示的尺寸大于等于摄像头输出的尺寸，退出裁剪
    }

    DCMI_X_Offset = Sensor_XSize - Displey_XSize;
    DCMI_Y_Offset = (Sensor_YSize - Displey_YSize) / 2 - 1;   // 寄存器值是从0开始计算的

    DCMI_CAPCNT = Displey_XSize * 2 - 1;   // 一个有效数据占2个字节，需要2个PCLK周期
    DCMI_VLINE  = Displey_YSize - 1;       // 垂直有效行数

    HAL_DCMI_ConfigCrop(&hdcmi, DCMI_X_Offset, DCMI_Y_Offset,
                        DCMI_CAPCNT, DCMI_VLINE);    // 设置裁剪参数
    HAL_DCMI_EnableCrop(&hdcmi);                      // 使能裁剪

    return OV5640_Success;
}

/***************************************************************************************************************************************
*   函 数 名: OV5640_Reset
*   函数功能: 执行软件复位
*****************************************************************************************************************************************/
void OV5640_Reset(void)
{
    HAL_Delay(20);

    OV5640_PWDN_OFF;  // PWDN 引脚输出低电平，不开启掉电模式
    HAL_Delay(20);
    DCMI_RST_H;
    HAL_Delay(10);
    HAL_Delay(20);

    SCCB_WriteReg_16Bit(0x3103, 0x11);
    SCCB_WriteReg_16Bit(0x3008, 0x82);
    HAL_Delay(5);
}

/***************************************************************************************************************************************
*   函 数 名: OV5640_ReadID
*   返 回 值: 器件ID
*   函数功能: 读取 OV5640 的器件ID
*****************************************************************************************************************************************/
uint16_t OV5640_ReadID(void)
{
    uint8_t PID_H, PID_L;     // ID高低字节

    PID_H = SCCB_ReadReg_16Bit(OV5640_ChipID_H); // 读取ID高字节
    PID_L = SCCB_ReadReg_16Bit(OV5640_ChipID_L); // 读取ID低字节

    return (PID_H << 8) | PID_L; // 拼接并返回器件ID
}

/***************************************************************************************************************************************
*   函 数 名: OV5640_Config
*   函数功能: 配置 OV5640 的各项参数
*   说    明: 配置表定义在 bsp_cam_cfg.h
*****************************************************************************************************************************************/
void OV5640_Config(void)
{
    uint32_t i;         // 循环变量
    uint8_t  read_reg;  // 读取值，用于调试

    for (i = 0; i < (sizeof(OV5640_INIT_Config) / 4); i++) {
        SCCB_WriteReg_16Bit(OV5640_INIT_Config[i][0],
                            OV5640_INIT_Config[i][1]);           // 写入数据

        read_reg = SCCB_ReadReg_16Bit(OV5640_INIT_Config[i][0]); // 读取配置，用于调试

        if (OV5640_INIT_Config[i][1] != read_reg) {              // 配置不成功
            printf("[CFG] ERR @ %lu: w 0x%04X->0x%02X, r 0x%02X\r\n",
                   (unsigned long)i,
                   OV5640_INIT_Config[i][0],
                   OV5640_INIT_Config[i][1], read_reg);
        }
    }
}

/***************************************************************************************************************************************
*   函 数 名: OV5640_Set_Pixformat
*   入口参数: pixformat - 像素格式，可选 Pixformat_RGB565、Pixformat_GRAY、Pixformat_JPEG
*   函数功能: 用于设置输出的格式
*****************************************************************************************************************************************/
void OV5640_Set_Pixformat(uint8_t pixformat)
{
    uint8_t OV5640_Reg;  // 寄存器值

    if (pixformat == Pixformat_JPEG) {
        SCCB_WriteReg_16Bit(OV5640_FORMAT_CONTROL,      0x30);
        SCCB_WriteReg_16Bit(OV5640_FORMAT_CONTROL_MUX,  0x00);
        SCCB_WriteReg_16Bit(OV5640_JPEG_MODE_SELECT,    0x02);
        SCCB_WriteReg_16Bit(OV5640_JPEG_VFIFO_CTRL00,   0xA0);
        SCCB_WriteReg_16Bit(OV5640_JPEG_VFIFO_HSIZE_H,  OV5640_Width >> 8);
        SCCB_WriteReg_16Bit(OV5640_JPEG_VFIFO_HSIZE_L,  (uint8_t)OV5640_Width);
        SCCB_WriteReg_16Bit(OV5640_JPEG_VFIFO_VSIZE_H,  OV5640_Height >> 8);
        SCCB_WriteReg_16Bit(OV5640_JPEG_VFIFO_VSIZE_L,  (uint8_t)OV5640_Height);
    } else if (pixformat == Pixformat_GRAY) {
        SCCB_WriteReg_16Bit(OV5640_FORMAT_CONTROL,      0x10);
        SCCB_WriteReg_16Bit(OV5640_FORMAT_CONTROL_MUX,  0x00);
    } else {  // RGB565
        SCCB_WriteReg_16Bit(OV5640_FORMAT_CONTROL,      0x6F);
        SCCB_WriteReg_16Bit(OV5640_FORMAT_CONTROL_MUX,  0x01);
    }

    OV5640_Reg = SCCB_ReadReg_16Bit(0x3821);
    SCCB_WriteReg_16Bit(0x3821,
        (OV5640_Reg & 0xDF) | ((pixformat == Pixformat_JPEG) ? 0x20 : 0x00));

    OV5640_Reg = SCCB_ReadReg_16Bit(0x3002);
    SCCB_WriteReg_16Bit(0x3002,
        (OV5640_Reg & 0xE3) | ((pixformat == Pixformat_JPEG) ? 0x00 : 0x1C));

    OV5640_Reg = SCCB_ReadReg_16Bit(0x3006);
    SCCB_WriteReg_16Bit(0x3006,
        (OV5640_Reg & 0xD7) | ((pixformat == Pixformat_JPEG) ? 0x28 : 0x00));
}

/***************************************************************************************************************************************
*   函 数 名: OV5640_Set_JPEG_QuantizationScale
*   入口参数: scale - 压缩等级，取值 0x01~0x3F
*   函数功能: 数值越大压缩率越高，得到的图片占用空间越小，但相应的画质会变差
*****************************************************************************************************************************************/
void OV5640_Set_JPEG_QuantizationScale(uint8_t scale)
{
    SCCB_WriteReg_16Bit(0x4407, scale);  // JPEG 压缩等级
}

/***************************************************************************************************************************************
*   函 数 名: OV5640_Set_Framesize
*   入口参数: width - 实际输出图像的长度，height - 实际输出图像的宽度
*   函数功能: 设置实际输出的图像大小（输出窗口）
*****************************************************************************************************************************************/
int8_t OV5640_Set_Framesize(uint16_t width, uint16_t height)
{
    // OV5640的很多参数设置需要按组的方式对应 group 寄存器
    SCCB_WriteReg_16Bit(OV5640_GroupAccess, 0X03);       // 开始 group 3 配置

    SCCB_WriteReg_16Bit(OV5640_TIMING_DVPHO_H, width >> 8);        // DVPHO输出图像水平尺寸
    SCCB_WriteReg_16Bit(OV5640_TIMING_DVPHO_L, width & 0xff);
    SCCB_WriteReg_16Bit(OV5640_TIMING_DVPVO_H, height >> 8);       // DVPVO输出图像垂直尺寸
    SCCB_WriteReg_16Bit(OV5640_TIMING_DVPVO_L, height & 0xff);

    SCCB_WriteReg_16Bit(OV5640_GroupAccess, 0X13);       // 结束组
    SCCB_WriteReg_16Bit(OV5640_GroupAccess, 0Xa3);       // 结束组

    return OV5640_Success;
}

/***************************************************************************************************************************************
*   函 数 名: OV5640_Set_Horizontal_Mirror
*   入口参数: ConfigState - 为1时，图像水平翻转，为0时恢复正常
*   函数功能: 用于设置输出的图像是否进行水平翻转
*****************************************************************************************************************************************/
int8_t OV5640_Set_Horizontal_Mirror(int8_t ConfigState)
{
    uint8_t OV5640_Reg;

    OV5640_Reg = SCCB_ReadReg_16Bit(OV5640_TIMING_Mirror);   // 读取寄存器值

    if (ConfigState == OV5640_Enable)     // 使能镜像
        OV5640_Reg |= 0X06;
    else                                  // 取消镜像
        OV5640_Reg &= 0xF9;

    return SCCB_WriteReg_16Bit(OV5640_TIMING_Mirror, OV5640_Reg);
}

/***************************************************************************************************************************************
*   函 数 名: OV5640_Set_Vertical_Flip
*   入口参数: ConfigState - 为1时，图像垂直翻转，为0时恢复正常
*   函数功能: 用于设置输出的图像是否进行垂直翻转
*****************************************************************************************************************************************/
int8_t OV5640_Set_Vertical_Flip(int8_t ConfigState)
{
    uint8_t OV5640_Reg;

    OV5640_Reg = SCCB_ReadReg_16Bit(OV5640_TIMING_Flip);   // 读取寄存器值

    if (ConfigState == OV5640_Enable)     // 使能翻转
        OV5640_Reg |= 0X06;
    else                                  // 取消翻转
        OV5640_Reg &= 0xF9;

    return SCCB_WriteReg_16Bit(OV5640_TIMING_Flip, OV5640_Reg);
}

/***************************************************************************************************************************************
*   函 数 名: OV5640_Set_Brightness
*   入口参数: Brightness - 亮度，9个等级：4,3,2,1,0,-1,-2,-3,-4  数值越大亮度越高
*****************************************************************************************************************************************/
void OV5640_Set_Brightness(int8_t Brightness)
{
    Brightness = Brightness + 4;
    SCCB_WriteReg_16Bit(OV5640_GroupAccess, 0X03);   // 开始 group 3 配置

    SCCB_WriteReg_16Bit(0x5587, OV5640_Brightness_Config[Brightness][0]);
    SCCB_WriteReg_16Bit(0x5588, OV5640_Brightness_Config[Brightness][1]);

    SCCB_WriteReg_16Bit(OV5640_GroupAccess, 0X13);   // 结束组
    SCCB_WriteReg_16Bit(OV5640_GroupAccess, 0Xa3);   // 结束组
}

/***************************************************************************************************************************************
*   函 数 名: OV5640_Set_Contrast
*   入口参数: Contrast - 对比度，7个等级：3,2,1,0,-1,-2,-3
*****************************************************************************************************************************************/
void OV5640_Set_Contrast(int8_t Contrast)
{
    Contrast = Contrast + 3;
    SCCB_WriteReg_16Bit(OV5640_GroupAccess, 0X03);   // 开始 group 3 配置

    SCCB_WriteReg_16Bit(0x5586, OV5640_Contrast_Config[Contrast][0]);
    SCCB_WriteReg_16Bit(0x5585, OV5640_Contrast_Config[Contrast][1]);

    SCCB_WriteReg_16Bit(OV5640_GroupAccess, 0X13);   // 结束组
    SCCB_WriteReg_16Bit(OV5640_GroupAccess, 0Xa3);   // 结束组
}

/***************************************************************************************************************************************
*   函 数 名: OV5640_Set_Effect
*   入口参数: effect_Mode - 特效模式，可选 OV5640_Effect_Normal、Negative、BW、Solarize
*   函数功能: 用于设置OV5640的特效，支持正常、负片、黑白、正负片叠加模式
*****************************************************************************************************************************************/
void OV5640_Set_Effect(uint8_t effect_Mode)
{
    SCCB_WriteReg_16Bit(OV5640_GroupAccess, 0X03);   // 开始 group 3 配置

    SCCB_WriteReg_16Bit(0x5580, OV5640_Effect_Config[effect_Mode][0]);
    SCCB_WriteReg_16Bit(0x5583, OV5640_Effect_Config[effect_Mode][1]);
    SCCB_WriteReg_16Bit(0x5584, OV5640_Effect_Config[effect_Mode][2]);
    SCCB_WriteReg_16Bit(0x5003, OV5640_Effect_Config[effect_Mode][3]);

    SCCB_WriteReg_16Bit(OV5640_GroupAccess, 0X13);   // 结束组
    SCCB_WriteReg_16Bit(OV5640_GroupAccess, 0Xa3);   // 结束组
}

/***************************************************************************************************************************************
*   函 数 名: OV5640_AF_Download_Firmware
*   函数功能: 将自动对焦固件写入OV5640
*   说    明: 因为OV5640片内没有flash，不能保存固件，所以每次上电都要写入一次
*****************************************************************************************************************************************/
int8_t OV5640_AF_Download_Firmware(void)
{
    uint8_t  AF_Status = 0;          // 对焦状态
    uint16_t i = 0;                  // 循环变量
    uint16_t OV5640_MCU_Addr = 0x8000;   // OV5640 MCU 存储器起始地址为 0x8000

    SCCB_WriteReg_16Bit(0x3000, 0x20);   // Bit[5]复位MCU，写固件之前需要执行此操作
    SCCB_WriteBuffer_16Bit(OV5640_MCU_Addr,
                           (uint8_t *)OV5640_AF_Firmware,
                           sizeof(OV5640_AF_Firmware));
    SCCB_WriteReg_16Bit(0x3000, 0x00);   // Bit[5]写操作结束，写0启动MCU

    for (i = 0; i < 100; i++) {
        AF_Status = SCCB_ReadReg_16Bit(OV5640_AF_FW_STATUS);
        if (AF_Status == 0x7E) {
            // printf("AF固件初始化中>>>\r\n");
        }
        if (AF_Status == 0x70) {
            // printf("AF固件写入成功！！\r\n");
            return OV5640_Success;
        }
    }
    // printf("自动对焦固件写入失败，返回error状态\r\n");
    return OV5640_Error;
}

/***************************************************************************************************************************************
*   函 数 名: OV5640_AF_QueryStatus
*   返 回 值: OV5640_AF_End - 对焦结束, OV5640_AF_Focusing - 正在对焦
*   函数功能: 对焦状态查询
*****************************************************************************************************************************************/
int8_t OV5640_AF_QueryStatus(void)
{
    uint8_t AF_Status = 0;

    AF_Status = SCCB_ReadReg_16Bit(OV5640_AF_FW_STATUS);
    // printf("AF_Status:0x%x\r\n", AF_Status);

    if ((AF_Status == 0x10) || (AF_Status == 0x20))
        return OV5640_AF_End;
    else
        return OV5640_AF_Focusing;
}

/***************************************************************************************************************************************
*   函 数 名: OV5640_AF_Trigger_Constant
*   函数功能: 自动对焦（持续触发），OV5640检测到当前画面不在焦点时会一直自动对焦
*****************************************************************************************************************************************/
void OV5640_AF_Trigger_Constant(void)
{
    SCCB_WriteReg_16Bit(0x3022, 0x04);   // 持续对焦
}

/***************************************************************************************************************************************
*   函 数 名: OV5640_AF_Trigger_Single
*   函数功能: 触发一次自动对焦
*****************************************************************************************************************************************/
void OV5640_AF_Trigger_Single(void)
{
    SCCB_WriteReg_16Bit(OV5640_AF_CMD_MAIN, 0x03);   // 触发一次自动对焦
}

/***************************************************************************************************************************************
*   函 数 名: OV5640_AF_Release
*   函数功能: 释放马达，镜头回到初始（对焦为无穷远处）位置
*****************************************************************************************************************************************/
void OV5640_AF_Release(void)
{
    SCCB_WriteReg_16Bit(OV5640_AF_CMD_MAIN, 0x08);   // 对焦释放指令
}

/***************************************************************************************************************************************
*   函 数 名: HAL_DCMI_FrameEventCallback
*   函数功能: 帧回调函数，每传输完一帧数据，由中断服务函数调用
*   说    明: 每次传输完一帧数据后，置位标志，并计算帧率
*****************************************************************************************************************************************/
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi)
{
    static uint32_t DCMI_Tick = 0;
    static uint8_t  DCMI_Frame_Count = 0;

    if (HAL_GetTick() - DCMI_Tick >= 1000) {
        DCMI_Tick = HAL_GetTick();
        OV5640_FPS = DCMI_Frame_Count;
        DCMI_Frame_Count = 0;
    }
    DCMI_Frame_Count++;

    OV5640_FrameState = 1;
}

/***************************************************************************************************************************************
*   函 数 名: HAL_DCMI_ErrorCallback
*   函数功能: 错误回调函数
*****************************************************************************************************************************************/
void HAL_DCMI_ErrorCallback(DCMI_HandleTypeDef *hdcmi)
{
    (void)hdcmi;
}

/* ====================== JPEG 硬件编解码 ====================== */

void HAL_JPEG_MspInit(JPEG_HandleTypeDef *hjpeg)
{
    (void)hjpeg;
    __HAL_RCC_JPGDECEN_CLK_ENABLE();
}

int8_t OV5640_JPEG_HW_Init(void)
{
    hjpeg.Instance = JPEG;
    if (HAL_JPEG_Init(&hjpeg) != HAL_OK) {
        return OV5640_Error;
    }
    /* 设置解码输出格式为 RGB565（默认 ARGB8888 会浪费 2 字节/像素） */
    hjpeg.Instance->CONFR4 = (hjpeg.Instance->CONFR4 & ~0x3U) | 0x2U;
    return OV5640_Success;
}

int8_t DCMI_OV5640_JPEG_Init(void)
{
    /* ---- DMA CIRCULAR 连续模式 ---- */
    __HAL_RCC_DMA2_CLK_ENABLE();

    DMA_Handle_dcmi.Instance                 = DMA2_Stream7;
    DMA_Handle_dcmi.Init.Request             = DMA_REQUEST_DCMI;
    DMA_Handle_dcmi.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    DMA_Handle_dcmi.Init.PeriphInc           = DMA_PINC_DISABLE;
    DMA_Handle_dcmi.Init.MemInc              = DMA_MINC_ENABLE;
    DMA_Handle_dcmi.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    DMA_Handle_dcmi.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
    DMA_Handle_dcmi.Init.Mode                = DMA_CIRCULAR;
    DMA_Handle_dcmi.Init.Priority            = DMA_PRIORITY_LOW;
    DMA_Handle_dcmi.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
    DMA_Handle_dcmi.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
    DMA_Handle_dcmi.Init.MemBurst            = DMA_MBURST_SINGLE;
    DMA_Handle_dcmi.Init.PeriphBurst         = DMA_PBURST_SINGLE;
    HAL_DMA_Init(&DMA_Handle_dcmi);
    __HAL_LINKDMA(&hdcmi, DMA_Handle, DMA_Handle_dcmi);

    HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);

    /* ---- DCMI JPEG 模式 ---- */
    hdcmi.Instance              = DCMI;
    hdcmi.Init.SynchroMode      = DCMI_SYNCHRO_HARDWARE;
    hdcmi.Init.PCKPolarity      = DCMI_PCKPOLARITY_RISING;
    hdcmi.Init.VSPolarity       = DCMI_VSPOLARITY_LOW;
    hdcmi.Init.HSPolarity       = DCMI_HSPOLARITY_LOW;
    hdcmi.Init.CaptureRate      = DCMI_CR_ALL_FRAME;
    hdcmi.Init.ExtendedDataMode = DCMI_EXTEND_DATA_8B;
    hdcmi.Init.JPEGMode         = DCMI_JPEG_ENABLE;
    hdcmi.Init.ByteSelectMode   = DCMI_BSM_ALL;
    hdcmi.Init.ByteSelectStart  = DCMI_OEBS_ODD;
    hdcmi.Init.LineSelectMode   = DCMI_LSM_ALL;
    hdcmi.Init.LineSelectStart  = DCMI_OELS_ODD;
    HAL_DCMI_Init(&hdcmi);

    HAL_NVIC_SetPriority(DCMI_IRQn, 0, 5);
    HAL_NVIC_EnableIRQ(DCMI_IRQn);

    /* ---- OV5640 上电 + JPEG 输出 ---- */
    OV5640_PWDN_OFF;
    HAL_Delay(140);
    DCMI_RST_H;
    HAL_Delay(40);

    SCCB_GPIO_Config();
    HAL_Delay(20);
    SCCB_WriteReg_16Bit(0x3103, 0x11);
    SCCB_WriteReg_16Bit(0x3008, 0x82);
    HAL_Delay(15);

    Device_ID = OV5640_ReadID();
    if (Device_ID == 0x5640) {
        OV5640_Config();
        OV5640_Set_Pixformat(Pixformat_JPEG);
        SCCB_WriteReg_16Bit(OV5640_JPEG_VFIFO_HSIZE_H, CAM_DISPLAY_Width >> 8);
        SCCB_WriteReg_16Bit(OV5640_JPEG_VFIFO_HSIZE_L, (uint8_t)CAM_DISPLAY_Width);
        SCCB_WriteReg_16Bit(OV5640_JPEG_VFIFO_VSIZE_H, CAM_DISPLAY_Height >> 8);
        SCCB_WriteReg_16Bit(OV5640_JPEG_VFIFO_VSIZE_L, (uint8_t)CAM_DISPLAY_Height);
        OV5640_Set_Framesize(CAM_DISPLAY_Width, CAM_DISPLAY_Height);
        return OV5640_Success;
    } else {
        return OV5640_Error;
    }
}

void OV5640_JPEG_Start_Continuous(void)
{
    DMA_Handle_dcmi.Init.Mode = DMA_CIRCULAR;
    HAL_DMA_Init(&DMA_Handle_dcmi);
    HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_CONTINUOUS,
                       (uint32_t)CAM_Buffer_ADDR,
                       CAM_DMA_BufferSize);
}

void OV5640_JPEG_StartSnapshot(void)
{
    DMA_Handle_dcmi.Init.Mode = DMA_NORMAL;
    HAL_DMA_Init(&DMA_Handle_dcmi);
    HAL_DCMI_Stop(&hdcmi);
    __HAL_DCMI_CLEAR_FLAG(&hdcmi, DCMI_FLAG_FRAMERI);
    HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_SNAPSHOT,
                       (uint32_t)CAM_Buffer_ADDR,
                       CAM_DMA_BufferSize);
}

uint32_t OV5640_JPEG_GetSnapshotSize(void)
{
    uint32_t remaining = __HAL_DMA_GET_COUNTER(&DMA_Handle_dcmi);
    return (CAM_DMA_BufferSize - remaining) * 4;
}

int8_t OV5640_JPEG_Decode(uint8_t *jpeg_data, uint32_t jpeg_size,
                          uint16_t *rgb_out, uint32_t rgb_out_size)
{
    if (HAL_JPEG_Decode(&hjpeg, jpeg_data, jpeg_size,
                        (uint8_t *)rgb_out, rgb_out_size,
                        1000) != HAL_OK) {
        return OV5640_Error;
    }
    return OV5640_Success;
}
