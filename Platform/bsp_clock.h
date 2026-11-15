#ifndef BSP_CLOCK_H
#define BSP_CLOCK_H

#include "gd32f4xx.h"

/* SYSCLK 由 SystemInit 配置；此处只保证 CK48M(PLLQ) 供 SDIO */
ErrStatus bsp_clock_init(void);

/* 外部低速时钟 LXTAL 32.768kHz（MCU OSC32_IN/OUT = PC14/PC15） */
ErrStatus bsp_lxtal_init(void);

/* 查询 LXTAL 是否已稳定 */
FlagStatus bsp_lxtal_ready(void);

#endif /* BSP_CLOCK_H */
