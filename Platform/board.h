#ifndef BOARD_H
#define BOARD_H

#include "gd32f4xx.h"

/* ========== 调试串口 USART0（官方 Printf + DMA 例程） ========== */
/* PA9=TX  PA10=RX  AF7  115200 8N1 */
/* TX: DMA1_CH7 SUBPERI4
 * RX: DMA1_CH2 SUBPERI4 —— 仅 WriteFile 高速模式占用；平时 CH2 给 SPI0 Flash/LCD
 *     高速模式期间 Flash 走轮询，避免与 USART RX DMA 抢 CH2
 */
#define BOARD_USART                 USART0
#define BOARD_USART_CLK             RCU_USART0
#define BOARD_USART_IRQn            USART0_IRQn
#define BOARD_USART_BAUDRATE        115200U
#define BOARD_USART_GPIO_PORT       GPIOA
#define BOARD_USART_GPIO_CLK        RCU_GPIOA
#define BOARD_USART_TX_PIN          GPIO_PIN_9
#define BOARD_USART_RX_PIN          GPIO_PIN_10
#define BOARD_USART_AF              GPIO_AF_7
#define BOARD_USART_DMA             DMA1
#define BOARD_USART_DMA_CLK         RCU_DMA1
#define BOARD_USART_DMA_TX_CH       DMA_CH7
#define BOARD_USART_DMA_RX_CH       DMA_CH2
#define BOARD_USART_DMA_SUBPERI     DMA_SUBPERI4
#define BOARD_USART_DATA_ADDR       ((uint32_t)&USART_DATA(USART0))

/* ========== 1.69" ST7789 + CST816T（扩展板原理图网标） ========== */
/* LCD 与 SPI Flash 共用 SPI0+DMA：PA5=SCK PA7=MOSI；CS 分时；Flash 优先 */
/* LCD: CS=PB0 DC=PB1 RST=PD6 BLK=PB10 */
/* TP : SCL=PB6 SDA=PB7(I2C0 总线) RST=PD7 INT=PB4 */
#define BOARD_LCD_SPI               SPI0
#define BOARD_LCD_SPI_CLK           RCU_SPI0
#define BOARD_LCD_SPI_AF            GPIO_AF_5
#define BOARD_LCD_SCK_PORT          GPIOA
#define BOARD_LCD_SCK_PIN           GPIO_PIN_5
#define BOARD_LCD_SCK_CLK           RCU_GPIOA
#define BOARD_LCD_MOSI_PORT         GPIOA
#define BOARD_LCD_MOSI_PIN          GPIO_PIN_7
#define BOARD_LCD_MOSI_CLK          RCU_GPIOA
#define BOARD_LCD_CS_PORT           GPIOB
#define BOARD_LCD_CS_PIN            GPIO_PIN_0
#define BOARD_LCD_CS_CLK            RCU_GPIOB
#define BOARD_LCD_DC_PORT           GPIOB
#define BOARD_LCD_DC_PIN            GPIO_PIN_1
#define BOARD_LCD_DC_CLK            RCU_GPIOB
#define BOARD_LCD_RST_PORT          GPIOD
#define BOARD_LCD_RST_PIN           GPIO_PIN_6
#define BOARD_LCD_RST_CLK           RCU_GPIOD
#define BOARD_LCD_BLK_PORT          GPIOB
#define BOARD_LCD_BLK_PIN           GPIO_PIN_10
#define BOARD_LCD_BLK_CLK           RCU_GPIOB

#define BOARD_TP_I2C_SCL_PORT       GPIOB
#define BOARD_TP_I2C_SCL_PIN        GPIO_PIN_6
#define BOARD_TP_I2C_SDA_PORT       GPIOB
#define BOARD_TP_I2C_SDA_PIN        GPIO_PIN_7
#define BOARD_TP_I2C_GPIO_CLK       RCU_GPIOB
#define BOARD_TP_RST_PORT           GPIOD
#define BOARD_TP_RST_PIN            GPIO_PIN_7
#define BOARD_TP_RST_CLK            RCU_GPIOD
#define BOARD_TP_INT_PORT           GPIOB
#define BOARD_TP_INT_PIN            GPIO_PIN_4
#define BOARD_TP_INT_CLK            RCU_GPIOB
#define BOARD_TP_EXTI_LINE          EXTI_4
#define BOARD_TP_EXTI_IRQn          EXTI4_IRQn
#define BOARD_TP_EXTI_PORT_SRC      EXTI_SOURCE_GPIOB
#define BOARD_TP_EXTI_PIN_SRC       EXTI_SOURCE_PIN4

/* ========== SPI Flash ZD25Q32D（与 W25Q32 指令兼容）========== */
/* PA4=CS(软)  PA5=SCK  PA6=MISO  PA7=MOSI → SPI0 AF5（与 LCD 共用总线） */
/* DMA：RX=DMA1_CH2 SUBPERI3  TX=DMA1_CH5 SUBPERI3（避开 SDIO CH3） */
#define BOARD_SPI_FLASH                 SPI0
#define BOARD_SPI_FLASH_CLK             RCU_SPI0
#define BOARD_SPI_FLASH_AF              GPIO_AF_5
#define BOARD_SPI_FLASH_GPIO_PORT       GPIOA
#define BOARD_SPI_FLASH_GPIO_CLK        RCU_GPIOA
#define BOARD_SPI_FLASH_CS_PIN          GPIO_PIN_4
#define BOARD_SPI_FLASH_SCK_PIN         GPIO_PIN_5
#define BOARD_SPI_FLASH_MISO_PIN        GPIO_PIN_6
#define BOARD_SPI_FLASH_MOSI_PIN        GPIO_PIN_7
#define BOARD_SPI_FLASH_DMA             DMA1
#define BOARD_SPI_FLASH_DMA_CLK         RCU_DMA1
#define BOARD_SPI_FLASH_DMA_RX_CH       DMA_CH2
#define BOARD_SPI_FLASH_DMA_TX_CH       DMA_CH5
#define BOARD_SPI_FLASH_DMA_SUBPERI     DMA_SUBPERI3

/* ========== USBFS（PA11=DM PA12=DP AF10）→ MSC U盘桥接 TF ========== */
#define BOARD_USB_DM_PIN            GPIO_PIN_11
#define BOARD_USB_DP_PIN            GPIO_PIN_12
#define BOARD_USB_GPIO_PORT         GPIOA
#define BOARD_USB_GPIO_CLK          RCU_GPIOA
#define BOARD_USB_AF                GPIO_AF_10

/* ========== 外部低速时钟 LXTAL（MCU PC14/PC15） ========== */
/* 注意：原理图中 PCF8563 的 32.768kHz 晶振属于该芯片自身，
 * 不是 MCU 的 LXTAL；MCU LXTAL 需接在 OSC32_IN/OUT(PC14/PC15)。 */

/* ========== 外挂 RTC PCF8563（I2C + INT） ========== */
#define BOARD_RTC_INT_PORT          GPIOB
#define BOARD_RTC_INT_PIN           GPIO_PIN_5
#define BOARD_RTC_INT_CLK           RCU_GPIOB
/* SDA/SCL 对应 MCU 引脚待原理图确认后补充 */

/* ========== 4x4 矩阵按键 ========== */
/* 行输出：PE8~PE11；列输入上拉：PE12~PE15 */
#define BOARD_KEY_GPIO_PORT         GPIOE
#define BOARD_KEY_GPIO_CLK          RCU_GPIOE
#define BOARD_KEY_ROW1_PIN          GPIO_PIN_8
#define BOARD_KEY_ROW2_PIN          GPIO_PIN_9
#define BOARD_KEY_ROW3_PIN          GPIO_PIN_10
#define BOARD_KEY_ROW4_PIN          GPIO_PIN_11
#define BOARD_KEY_COL1_PIN          GPIO_PIN_12
#define BOARD_KEY_COL2_PIN          GPIO_PIN_13
#define BOARD_KEY_COL3_PIN          GPIO_PIN_14
#define BOARD_KEY_COL4_PIN          GPIO_PIN_15
#define BOARD_KEY_ROW_PINS          (BOARD_KEY_ROW1_PIN | BOARD_KEY_ROW2_PIN | \
                                     BOARD_KEY_ROW3_PIN | BOARD_KEY_ROW4_PIN)
#define BOARD_KEY_COL_PINS          (BOARD_KEY_COL1_PIN | BOARD_KEY_COL2_PIN | \
                                     BOARD_KEY_COL3_PIN | BOARD_KEY_COL4_PIN)

/* ========== 双4位数码管（74HC595 三线：DI/SCK/RCK） ========== */
#define BOARD_NIX_GPIO_PORT         GPIOD
#define BOARD_NIX_GPIO_CLK          RCU_GPIOD
#define BOARD_NIX_DI_PIN            GPIO_PIN_0   /* 串行数据 */
#define BOARD_NIX_RCK_PIN           GPIO_PIN_1   /* 锁存 */
#define BOARD_NIX_SCK_PIN           GPIO_PIN_4   /* 移位时钟 */
/* 扫描定时器：500us 切一位，8 位整屏约 250Hz，减轻闪烁 */
#define BOARD_NIX_SCAN_TIMER        TIMER2
#define BOARD_NIX_SCAN_TIMER_CLK    RCU_TIMER2
#define BOARD_NIX_SCAN_IRQn         TIMER2_IRQn
/* 与参考驱动一致：先发段码(0点亮)，再发位选(1<<id)，SCK 上升沿移位 */

/* ========== 震动电机（PB8 = TIMER1_CH0 / AF1，PWM 占空比调强度） ========== */
#define BOARD_MOTOR_GPIO_PORT       GPIOB
#define BOARD_MOTOR_GPIO_PIN        GPIO_PIN_8
#define BOARD_MOTOR_GPIO_CLK        RCU_GPIOB
#define BOARD_MOTOR_TIMER_AF        GPIO_AF_1
#define BOARD_MOTOR_TIMER           TIMER1
#define BOARD_MOTOR_TIMER_CLK       RCU_TIMER1
#define BOARD_MOTOR_TIMER_CH        TIMER_CH_0
#define BOARD_MOTOR_PWM_FREQ_HZ     20000U

/* ========== 蜂鸣器（PB9 = TIMER3_CH3 / AF2，PWM 驱动无源蜂鸣器） ========== */
#define BOARD_BUZZER_GPIO_PORT      GPIOB
#define BOARD_BUZZER_GPIO_PIN       GPIO_PIN_9
#define BOARD_BUZZER_GPIO_CLK       RCU_GPIOB
#define BOARD_BUZZER_TIMER_AF       GPIO_AF_2

#define BOARD_BUZZER_TIMER          TIMER3
#define BOARD_BUZZER_TIMER_CLK      RCU_TIMER3
#define BOARD_BUZZER_TIMER_CH       TIMER_CH_3
#define BOARD_BUZZER_PWM_FREQ_HZ    4000U

/* ========== LED 指示灯 ========== */
/* LED_SW(PC6): PNP 总开关，低电平供电；LED1~8(PD8~PD15): 低电平点亮 */
#define BOARD_LED_SW_PORT           GPIOC
#define BOARD_LED_SW_PIN            GPIO_PIN_6
#define BOARD_LED_SW_CLK            RCU_GPIOC
#define BOARD_LED_GPIO_PORT         GPIOD
#define BOARD_LED_GPIO_CLK          RCU_GPIOD
#define BOARD_LED1_PIN              GPIO_PIN_8
#define BOARD_LED2_PIN              GPIO_PIN_9
#define BOARD_LED3_PIN              GPIO_PIN_10
#define BOARD_LED4_PIN              GPIO_PIN_11
#define BOARD_LED5_PIN              GPIO_PIN_12
#define BOARD_LED6_PIN              GPIO_PIN_13
#define BOARD_LED7_PIN              GPIO_PIN_14
#define BOARD_LED8_PIN              GPIO_PIN_15
#define BOARD_LED_PINS              (BOARD_LED1_PIN | BOARD_LED2_PIN | BOARD_LED3_PIN | BOARD_LED4_PIN | \
                                     BOARD_LED5_PIN | BOARD_LED6_PIN | BOARD_LED7_PIN | BOARD_LED8_PIN)

/* ========== 独立按键（低电平有效，需内部上拉） ========== */
/* S17->PC0, S18->PC1, S19->PC2, S20->PC3 */
#define BOARD_BTN_GPIO_PORT         GPIOC
#define BOARD_BTN_GPIO_CLK          RCU_GPIOC
#define BOARD_BTN_S17_PIN           GPIO_PIN_0
#define BOARD_BTN_S18_PIN           GPIO_PIN_1
#define BOARD_BTN_S19_PIN           GPIO_PIN_2
#define BOARD_BTN_S20_PIN           GPIO_PIN_3
#define BOARD_BTN_PINS              (BOARD_BTN_S17_PIN | BOARD_BTN_S18_PIN | \
                                     BOARD_BTN_S19_PIN | BOARD_BTN_S20_PIN)

/* ========== TF 卡（硬件 SDIO 4bit + DMA，官方 AF12） ========== */
/* D0=PC8 D1=PC9 D2=PC10 D3=PC11 CLK=PC12 CMD=PD2 DET=PD3 */
#define BOARD_TF_SDIO_DAT_PORT      GPIOC
#define BOARD_TF_SDIO_CLK_PORT      GPIOC
#define BOARD_TF_SDIO_CMD_PORT      GPIOD
#define BOARD_TF_DET_PORT           GPIOD
#define BOARD_TF_DET_PIN            GPIO_PIN_3
#define BOARD_TF_DET_CLK            RCU_GPIOD

/* ========== NTC（PA1 -> ADC012_IN1，上拉 10k + NTC 10k 分压） ========== */
#define BOARD_NTC_GPIO_PORT         GPIOA
#define BOARD_NTC_GPIO_PIN          GPIO_PIN_1
#define BOARD_NTC_GPIO_CLK          RCU_GPIOA
#define BOARD_NTC_ADC               ADC1
#define BOARD_NTC_ADC_CLK           RCU_ADC1
#define BOARD_NTC_ADC_CHANNEL       ADC_CHANNEL_1
#define BOARD_NTC_ADC_SAMPLETIME    ADC_SAMPLETIME_56
#define BOARD_NTC_PULLUP_OHM        10000U
#define BOARD_NTC_R25_OHM           10000U
#define BOARD_NTC_BETA              3950U

/* ========== 电位器硬件资源（PC4 -> ADC012_IN14） ========== */
#define BOARD_POT_GPIO_PORT         GPIOC
#define BOARD_POT_GPIO_PIN          GPIO_PIN_4
#define BOARD_POT_GPIO_CLK          RCU_GPIOC

#define BOARD_POT_ADC               ADC0
#define BOARD_POT_ADC_CLK           RCU_ADC0
#define BOARD_POT_ADC_CHANNEL       ADC_CHANNEL_14
#define BOARD_POT_ADC_SAMPLETIME    ADC_SAMPLETIME_56

#define BOARD_POT_DMA               DMA1
#define BOARD_POT_DMA_CLK           RCU_DMA1
#define BOARD_POT_DMA_CH            DMA_CH0
#define BOARD_POT_DMA_SUBPERI       DMA_SUBPERI0

#endif /* BOARD_H */
