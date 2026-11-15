#include "platform.h"
#include "board.h"
#include "systick.h"
#include <stdio.h>

void platform_init(void)
{
    /* FreeRTOS 推荐 4 位抢占优先级；外设 ISR 勿高于 configLIBRARY_MAX_SYSCALL(5) 若要调 FromISR */
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);

    bsp_clock_init();
    SystemCoreClockUpdate();

    systick_config();

    /* USART 尽早起来，便于看日志；LXTAL 放到后面，避免备份域超时拖死启动观感 */
    bsp_usart_init();
    printf("\r\n[GD32F407] USART0 PA9/PA10 @ %u\r\n", (unsigned)BOARD_USART_BAUDRATE);
    printf("[CLK] SystemCoreClock=%lu Hz\r\n", (unsigned long)SystemCoreClock);

    if(SUCCESS == bsp_lxtal_init()) {
        (void)bsp_rtc_init();
    } else {
        printf("[CLK] LXTAL fail, RTC skipped\r\n");
    }

    bsp_potentiometer_init();
    bsp_buzzer_init();
    bsp_keypad_init();
    bsp_button_init();
    bsp_led_init();
    bsp_ntc_init();
    bsp_motor_init();
    bsp_nixie_init();
    /* SPI Flash 在 app 里做 JEDEC ID 检测 */
}
