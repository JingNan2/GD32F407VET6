#ifndef BSP_USART_H
#define BSP_USART_H

#include "gd32f4xx.h"

/* PA9/PA10 USART0：官方 Dma_transmitter&receiver + Printf */
void     bsp_usart_init(void);
void     bsp_usart_send_byte(uint8_t ch);
void     bsp_usart_send_string(const char *s);
uint8_t  bsp_usart_dma_send(const uint8_t *buf, uint32_t len);
uint8_t  bsp_usart_dma_recv(uint8_t *buf, uint32_t len);

/* 非阻塞读一字节：1=有数据，0=空（普通模式环缓 / WriteFile 时 DMA 环） */
uint8_t  bsp_usart_getc(uint8_t *ch);
/* 批量取 RX（WriteFile 全速路径优先用这个） */
uint32_t bsp_usart_read(uint8_t *buf, uint32_t max_len);
/* 轮询收字节入环缓（中断已启用时作兜底；DMA 高速模式为空操作） */
void     bsp_usart_poll_rx(void);
/* USART0_IRQHandler 中调用 */
void     bsp_usart_rx_irq_handler(void);

/*
 * WriteFile 高速接收：DMA1_CH2 循环收 USART0
 * 期间 SPI0 Flash 不得占用 CH2（bsp_spi0_bus_dma_xfer 会自动改轮询）
 */
void     bsp_usart_rx_dma_highspeed_enter(void);
void     bsp_usart_rx_dma_highspeed_exit(void);
uint8_t  bsp_usart_rx_dma_is_active(void);

#endif /* BSP_USART_H */
