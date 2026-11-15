#ifndef GD32F4XX_IT_H
#define GD32F4XX_IT_H

#include "gd32f4xx.h"

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void DebugMon_Handler(void);
void SysTick_Handler(void);
/* SVC/PendSV：仅 FreeRTOS port 需要 */
void TIMER2_IRQHandler(void);
void SDIO_IRQHandler(void);
void USBFS_IRQHandler(void);
void EXTI4_IRQHandler(void);
void USART0_IRQHandler(void);

#endif /* GD32F4XX_IT_H */
