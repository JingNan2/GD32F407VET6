/*!
    \file    freertos_heap_ccm.c
    \brief   FreeRTOS heap_4 内存池放到 CCM（0x10000000）

    仅在工程编入 FreeRTOS 时需要本文件；切 BARE 且剔除 FreeRTOS 组时一并排除。
*/

#include "FreeRTOS.h"

#if (configAPPLICATION_ALLOCATED_HEAP == 1)
#if defined(__ARMCC_VERSION) || defined(__CC_ARM) || defined(__GNUC__)
uint8_t ucHeap[configTOTAL_HEAP_SIZE] __attribute__((section(".ccmram")));
#else
uint8_t ucHeap[configTOTAL_HEAP_SIZE];
#endif
#endif
