/**
 * ===========================================================================
 * BSP 驱动层 — OV5640 摄像头 (DCMI + SCCB)
 * ===========================================================================
 *
 * DCMI 引脚映射：
 *   PC6=D0  PC7=D1  PE0=D2  PE1=D3  PE4=D4  PD3=D5  PE5=D6  PE6=D7
 *   PB7=VSYNC  PA4=HSYNC  PA6=PIXCLK
 *
 * SCCB 引脚映射（模拟I2C）：
 *   PB6=SCL  PB9=SDA
 *
 * 控制引脚：
 *   PD10=PWDN  PE13=RST  PE12=CAM_LED
 */
#ifndef __BSP_CAM_H
#define __BSP_CAM_H

#include "stm32h7xx_hal.h"

/*----------------------------------------- SCCB 引脚配置宏 -----------------------------------------------*/

#define SCCB_SCL_CLK_ENABLE       __HAL_RCC_GPIOB_CLK_ENABLE()     // SCL 引脚时钟
#define SCCB_SCL_PORT              GPIOB                            // SCL 引脚端口
#define SCCB_SCL_PIN               GPIO_PIN_6                       // SCL 引脚

#define SCCB_SDA_CLK_ENABLE       __HAL_RCC_GPIOB_CLK_ENABLE()     // SDA 引脚时钟
#define SCCB_SDA_PORT              GPIOB                            // SDA 引脚端口
#define SCCB_SDA_PIN               GPIO_PIN_9                       // SDA 引脚

/*------------------------------------------ IIC相关定义 -------------------------------------------------*/

#define ACK_OK      1            // 响应正常
#define ACK_ERR     0            // 响应错误

// SCCB通信延时，SCCB_Delay()函数使用，
// 通信速度在300KHz左右
#define SCCB_DelayVaule  100//8

/*-------------------------------------------- IO口操作 ---------------------------------------------------*/

#define SCCB_SCL(a) if (a) \
    HAL_GPIO_WritePin(SCCB_SCL_PORT, SCCB_SCL_PIN, GPIO_PIN_SET); \
    else \
    HAL_GPIO_WritePin(SCCB_SCL_PORT, SCCB_SCL_PIN, GPIO_PIN_RESET)

#define SCCB_SDA(a) if (a) \
    HAL_GPIO_WritePin(SCCB_SDA_PORT, SCCB_SDA_PIN, GPIO_PIN_SET); \
    else \
    HAL_GPIO_WritePin(SCCB_SDA_PORT, SCCB_SDA_PIN, GPIO_PIN_RESET)

/*----------------------------------------- OV5640 器件地址 -----------------------------------------------*/

#define OV2640_DEVICE_ADDRESS     0x60    // OV2640地址
#define OV5640_DEVICE_ADDRESS     0X78    // OV5640地址

/*----------------------------------------- 图像尺寸配置 --------------------------------------------------*/

// 1. 定义OV5640实际输出的图像大小，可以根据实际的应用或者显示屏进行调整
// 2. 这两个参数不会影响帧率
// 3. 因为配置的OV5640的ISP窗口比例为4:3(1280*960)，用户设置的输出尺寸也应满足这个比例
// 4. 如果需要其它比例，需要修改初始化配置里的参数
#define OV5640_Width              400    // 图像长度
#define OV5640_Height             300    // 图像宽度

// 1. 定义要显示的画面大小，数值一定要能被4整除！！
// 2. RGB565格式下，最终会由DCMI将OV5640输出的4:3图像裁剪为适应屏幕的比例
// 3. JPG模式下，数值一定要能被8整除！！
#define CAM_DISPLAY_Width         240    // 显示图像长度
#define CAM_DISPLAY_Height        280    // 显示图像宽度

// DMA传输数据大小（32位宽），宽度*高度*2(字节/像素)/4
// 注意：DCMI裁剪后实际输出为 Display 尺寸，DMA buffer 按此配置
#define CAM_DMA_BufferSize        (CAM_DISPLAY_Width * CAM_DISPLAY_Height * 2 / 4)
#define CAM_DISP_BufferSize       CAM_DMA_BufferSize

/*----------------------------------------- 摄像头缓冲区（AXI SRAM） --------------------------------------*/
/* 地址由链接脚本 .cam_buf / .cam_jpg_buf 段自动分配，紧随 .dma_bss */

extern uint32_t __cam_buf_start;
extern uint32_t __cam_jpg_buf_start;

#define CAM_Buffer_ADDR          ((uint32_t)&__cam_buf_start)
#define CAM_Buffer               ((uint32_t)CAM_Buffer_ADDR)
#define CAM_RGB_BUF_ADDR         ((uint32_t)&__cam_jpg_buf_start)   // JPEG 解码用（暂不使用）

/*----------------------------------------- 像素格式选择 --------------------------------------------------*/

// 用于设置输出的格式，被 OV5640_Set_Pixformat() 引用
#define Pixformat_RGB565           0
#define Pixformat_JPEG             1
#define Pixformat_GRAY             2

/*----------------------------------------- 状态码定义 ----------------------------------------------------*/

#define OV5640_Success             0           // 通讯成功标志
#define OV5640_Error              -1           // 通讯错误

#define OV5640_Enable              1
#define OV5640_Disable             0

#define OV5640_AF_Focusing         2           // 正在处于自动对焦中
#define OV5640_AF_End              1           // 自动对焦结束

/*----------------------------------------- 特效模式 ------------------------------------------------------*/

// OV5640的特效模式，被 OV5640_Set_Effect() 引用
#define OV5640_Effect_Normal       0           // 正常模式
#define OV5640_Effect_Negative     1           // 负片模式，也就是颜色全部取反
#define OV5640_Effect_BW           2           // 黑白模式
#define OV5640_Effect_Solarize     3           // 正负片叠加模式

/*----------------------------------------- 控制引脚宏 ----------------------------------------------------*/

// PWDN GPIO端口
#define GPIO_OV5640_PWDN_CLK_ENABLE   __HAL_RCC_GPIOD_CLK_ENABLE()   // PWDN GPIO端口时钟

// 低电平，不开启掉电模式，摄像头正常工作
#define OV5640_PWDN_OFF   HAL_GPIO_WritePin(GPIOD, GPIO_PIN_10, GPIO_PIN_RESET)

// 高电平，进入掉电模式，摄像头停止工作，此时功耗降到最低
#define OV5640_PWDN_ON    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_10, GPIO_PIN_SET)

#define DCMI_RST_H        (GPIOE->ODR |= GPIO_PIN_13)
#define DCMI_RST_L        (GPIOE->ODR &= ~GPIO_PIN_13)

// 低电平，关闭摄像头补光LED
#define CAM_LED_OFF       HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_RESET)
// 高电平，开启摄像头补光LED
#define CAM_LED_ON        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_SET)

/*---------------- 常用寄存器 -----------------------------*/

#define OV5640_ChipID_H            0x300A   // 芯片ID寄存器 高字节
#define OV5640_ChipID_L            0x300B   // 芯片ID寄存器 低字节

#define OV5640_FORMAT_CONTROL      0x4300   // 设置数据接口输出的格式
#define OV5640_FORMAT_CONTROL_MUX  0x501F   // 设置ISP的格式

#define OV5640_JPEG_MODE_SELECT    0x4713   // JPEG模式选择
#define OV5640_JPEG_VFIFO_CTRL00   0x4600   // 用于设置JPEG模式2是否固定输出宽度
#define OV5640_JPEG_VFIFO_HSIZE_H  0x4602   // JPEG输出水平尺寸,高字节
#define OV5640_JPEG_VFIFO_HSIZE_L  0x4603   // JPEG输出水平尺寸,低字节
#define OV5640_JPEG_VFIFO_VSIZE_H  0x4604   // JPEG输出垂直尺寸,低字节
#define OV5640_JPEG_VFIFO_VSIZE_L  0x4605   // JPEG输出垂直尺寸,低字节

#define OV5640_GroupAccess         0X3212   // 寄存器组访问
#define OV5640_TIMING_DVPHO_H      0x3808   // 输出水平尺寸,高字节
#define OV5640_TIMING_DVPHO_L      0x3809   // 输出水平尺寸,低字节
#define OV5640_TIMING_DVPVO_H      0x380A   // 输出垂直尺寸,高字节
#define OV5640_TIMING_DVPVO_L      0x380B   // 输出垂直尺寸,低字节
#define OV5640_TIMING_Flip         0x3820   // Bit[2:1]用于设置是否垂直翻转
#define OV5640_TIMING_Mirror       0x3821   // Bit[2:1]用于设置是否水平镜像

#define OV5640_AF_CMD_MAIN         0x3022   // AF 主命令
#define OV5640_AF_CMD_ACK          0x3023   // AF 命令确认
#define OV5640_AF_FW_STATUS        0x3029   // 对焦状态寄存器

/*----------------------------------------- 外部变量 ------------------------------------------------------*/

// DCMI状态标志，当数据帧传输完成时，会被 HAL_DCMI_FrameEventCallback() 中断回调函数置 1
extern volatile uint8_t  OV5640_FrameState;    // DCMI 帧就绪标志
extern volatile uint8_t  OV5640_FPS;            // 帧率
extern DCMI_HandleTypeDef   hdcmi;              // DCMI句柄
extern DMA_HandleTypeDef    DMA_Handle_dcmi;    // DMA句柄
extern JPEG_HandleTypeDef   hjpeg;              // JPEG硬件编解码句柄

/*--------------------------------------------- SCCB 函数声明 ----------------------------------------------*/

void     SCCB_GPIO_Config(void);                              // IIC引脚初始化
void     SCCB_Delay(uint32_t a);                              // IIC延时函数
void     SCCB_Start(void);                                    // 启动IIC通信
void     SCCB_Stop(void);                                     // IIC停止信号
void     SCCB_ACK(void);                                      // 发送响应信号
void     SCCB_NoACK(void);                                    // 发送非应答信号
uint8_t  SCCB_WaitACK(void);                                  // 等待应答信号
uint8_t  SCCB_WriteByte(uint8_t IIC_Data);                    // 写字节函数
uint8_t  SCCB_ReadByte(uint8_t ACK_Mode);                     // 读字节函数

uint8_t  SCCB_WriteReg(uint8_t addr, uint8_t value);          // 对指定的寄存器(8位地址)写一字节数据，OV2640用到
uint8_t  SCCB_ReadReg(uint8_t addr);                          // 对指定的寄存器(8位地址)读一字节数据，OV2640用到

uint8_t  SCCB_WriteReg_16Bit(uint16_t addr, uint8_t value);   // 对指定的寄存器(16位地址)写一字节数据，OV5640用到
uint8_t  SCCB_ReadReg_16Bit(uint16_t addr);                   // 对指定的寄存器(16位地址)读一字节数据，OV5640用到
uint8_t  SCCB_WriteBuffer_16Bit(uint16_t addr, uint8_t *pData, uint32_t size); // 对指定的寄存器(16位地址)批量写数据，OV5640 写入自动对焦固件时用到

/*------------------------------------------ DCMI / DMA 函数声明 -------------------------------------------*/

void     HAL_DCMI_MspInit(DCMI_HandleTypeDef* hdcmi);         // 初始化 DCMI 引脚
void     MX_DCMI_Init(void);                                  // 配置DCMI相关参数
void     OV5640_DMA_Init(void);                               // 配置 DMA 相关参数

/*------------------------------------------ OV5640 操作函数 -----------------------------------------------*/

int8_t   DCMI_OV5640_Init(void);                              // 初始SCCB、DCMI、DMA以及配置OV5640

void     OV5640_DMA_Transmit_Continuous(uint32_t DMA_Buffer, uint32_t DMA_BufferSize);  // 启动DMA传输，连续模式
void     OV5640_DMA_Transmit_Snapshot(uint32_t DMA_Buffer, uint32_t DMA_BufferSize);    // 启动DMA传输，快照模式，传输一帧图像后停止
void     OV5640_DCMI_Suspend(void);                           // 挂起DCMI，停止捕获数据
void     OV5640_DCMI_Resume(void);                            // 恢复DCMI，开始捕获数据
void     OV5640_DCMI_Stop(void);                              // 禁止DCMI的DMA请求，停止DCMI捕获，禁止DCMI外设
int8_t   OV5640_DCMI_Crop(uint16_t Displey_XSize, uint16_t Displey_YSize,
                           uint16_t Sensor_XSize, uint16_t Sensor_YSize);  // 裁剪画面

void     OV5640_Reset(void);                                  // 执行软件复位
uint16_t OV5640_ReadID(void);                                 // 读取器件ID
void     OV5640_Config(void);                                 // 配置OV5640各项参数

void     OV5640_Set_Pixformat(uint8_t pixformat);             // 设置图像输出格式
void     OV5640_Set_JPEG_QuantizationScale(uint8_t scale);    // 设置JPEG格式的压缩等级,取值 0x01~0x3F
int8_t   OV5640_Set_Framesize(uint16_t width, uint16_t height);  // 设置实际输出的图像大小
int8_t   OV5640_Set_Horizontal_Mirror(int8_t ConfigState);    // 用于设置输出的图像是否进行水平镜像
int8_t   OV5640_Set_Vertical_Flip(int8_t ConfigState);        // 用于设置输出的图像是否进行垂直翻转
void     OV5640_Set_Brightness(int8_t Brightness);            // 设置亮度
void     OV5640_Set_Contrast(int8_t Contrast);                // 设置对比度
void     OV5640_Set_Effect(uint8_t effect_Mode);              // 用于设置特效

int8_t   OV5640_AF_Download_Firmware(void);                   // 将自动对焦固件写入OV5640
int8_t   OV5640_AF_QueryStatus(void);                         // 对焦状态查询
void     OV5640_AF_Trigger_Constant(void);                    // 自动对焦 ，持续 触发
void     OV5640_AF_Trigger_Single(void);                      // 自动对焦 ，触发 单次
void     OV5640_AF_Release(void);                             // 释放马达，镜头回到初始位置

/*------------------------------------------ JPEG 硬件编解码 -----------------------------------------------*/

int8_t   OV5640_JPEG_HW_Init(void);                                 // 初始化 JPEG 硬件解码器
int8_t   DCMI_OV5640_JPEG_Init(void);                               // JPEG 模式初始化（DMA NORMAL + DCMI）
void     OV5640_JPEG_StartSnapshot(void);                           // 启动一帧 JPEG 快照
uint32_t OV5640_JPEG_GetSnapshotSize(void);                         // 快照实际字节数
int8_t   OV5640_JPEG_Decode(uint8_t *jpeg_data, uint32_t jpeg_size,
                            uint16_t *rgb_out, uint32_t rgb_out_size); // 硬件 JPEG 解码→RGB565

/*------------------------------------------ 中断回调 ------------------------------------------------------*/

void     HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi);    // 帧事件回调
void     HAL_DCMI_ErrorCallback(DCMI_HandleTypeDef *hdcmi);         // 错误回调

#endif //__BSP_CAM_H
