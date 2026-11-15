#ifndef BSP_USART_H
#define BSP_USART_H

#include "gd32f4xx.h"

/* PA9/PA10 USART0：官方 Dma_transmitter&receiver + Printf */
void     bsp_usart_init(void);
void     bsp_usart_send_byte(uint8_t ch);
void     bsp_usart_send_string(const char *s);
uint8_t  bsp_usart_dma_send(const uint8_t *buf, uint32_t len);
uint8_t  bsp_usart_dma_recv(uint8_t *buf, uint32_t len);

/* 非阻塞读一字节：1=有数据，0=空 */
uint8_t  bsp_usart_getc(uint8_t *ch);
/* 轮询收字节入环缓（中断已启用时作兜底） */
void     bsp_usart_poll_rx(void);
/* USART0_IRQHandler 中调用 */
void     bsp_usart_rx_irq_handler(void);

#endif /* BSP_USART_H */
