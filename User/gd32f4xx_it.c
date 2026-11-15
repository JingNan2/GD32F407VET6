#include "gd32f4xx_it.h"
#include "systick.h"
#include "bsp_nixie.h"
#include "board.h"
#include "sdcard.h"
#include "bsp_usb_msc.h"
#include "bsp_cst816.h"
#include "bsp_usart.h"
#include "drv_usbd_int.h"
#include "os.h"

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
    while(1) {
    }
}

void MemManage_Handler(void)
{
    while(1) {
    }
}

void BusFault_Handler(void)
{
    while(1) {
    }
}

void UsageFault_Handler(void)
{
    while(1) {
    }
}

void DebugMon_Handler(void)
{
}

/* SVC / PendSV：仅 FreeRTOS port 提供；BARE 模式下若链接报缺符号需保证不编入 FreeRTOS port */

void SysTick_Handler(void)
{
    os_sys_tick_handler();
}

void USART0_IRQHandler(void)
{
    bsp_usart_rx_irq_handler();
}

void TIMER2_IRQHandler(void)
{
    if(RESET != timer_interrupt_flag_get(BOARD_NIX_SCAN_TIMER, TIMER_INT_FLAG_UP)) {
        timer_interrupt_flag_clear(BOARD_NIX_SCAN_TIMER, TIMER_INT_FLAG_UP);
        bsp_nixie_task();
    }
}

void SDIO_IRQHandler(void)
{
    (void)sd_interrupts_process();
}

void USBFS_IRQHandler(void)
{
    usbd_isr(&g_msc_udisk);
}

void EXTI4_IRQHandler(void)
{
    bsp_cst816_exti_isr();
}
