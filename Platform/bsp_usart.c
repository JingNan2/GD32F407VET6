/*!
    \file    bsp_usart.c
    \brief   USART0 PA9/PA10 — Printf + DMA 发送

    TX: DMA1 CH7 / SUBPERI4
    RX: 暂不用 DMA（DMA1_CH2 留给 SPI0 Flash/LCD）
    printf 行缓冲后走 DMA 发送；单字节仍可轮询。
*/

#include "bsp_usart.h"
#include "board.h"
#include <stdio.h>
#include <string.h>

#define USART_PRINTF_BUF_SIZE  256U
#define USART_DMA_TIMEOUT      0x1FFFFFU
#define USART_RX_RING_SIZE     128U

static uint8_t  s_printf_buf[USART_PRINTF_BUF_SIZE];
static uint16_t s_printf_len = 0U;

static uint8_t  s_rx_ring[USART_RX_RING_SIZE];
static volatile uint16_t s_rx_head = 0U;
static volatile uint16_t s_rx_tail = 0U;

static void rx_push(uint8_t ch)
{
    uint16_t next = (uint16_t)((s_rx_head + 1U) % USART_RX_RING_SIZE);
    if(next != s_rx_tail) {
        s_rx_ring[s_rx_head] = ch;
        s_rx_head = next;
    }
}

static void usart_gpio_config(void)
{
    rcu_periph_clock_enable(BOARD_USART_GPIO_CLK);
    rcu_periph_clock_enable(BOARD_USART_CLK);

    gpio_af_set(BOARD_USART_GPIO_PORT, BOARD_USART_AF, BOARD_USART_TX_PIN);
    gpio_af_set(BOARD_USART_GPIO_PORT, BOARD_USART_AF, BOARD_USART_RX_PIN);

    gpio_mode_set(BOARD_USART_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, BOARD_USART_TX_PIN);
    gpio_output_options_set(BOARD_USART_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, BOARD_USART_TX_PIN);

    gpio_mode_set(BOARD_USART_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, BOARD_USART_RX_PIN);
    gpio_output_options_set(BOARD_USART_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, BOARD_USART_RX_PIN);

    usart_deinit(BOARD_USART);
    usart_baudrate_set(BOARD_USART, BOARD_USART_BAUDRATE);
    usart_receive_config(BOARD_USART, USART_RECEIVE_ENABLE);
    usart_transmit_config(BOARD_USART, USART_TRANSMIT_ENABLE);
    usart_enable(BOARD_USART);
}

void bsp_usart_init(void)
{
    s_printf_len = 0U;
    s_rx_head = 0U;
    s_rx_tail = 0U;
    rcu_periph_clock_enable(BOARD_USART_DMA_CLK);
    usart_gpio_config();

    /* RBNE 中断收包，避免回显/DMA 发 TX 时丢掉突发 RX */
    nvic_irq_enable(BOARD_USART_IRQn, 6U, 0U);
    usart_interrupt_enable(BOARD_USART, USART_INT_RBNE);
}

/*!
    \brief  官方 DMA1_CH7 发送一块数据（阻塞等到 FTF）
*/
uint8_t bsp_usart_dma_send(const uint8_t *buf, uint32_t len)
{
    dma_single_data_parameter_struct dma_init_struct;
    uint32_t timeout = USART_DMA_TIMEOUT;

    if((NULL == buf) || (0U == len)) {
        return 1U;
    }

    dma_channel_disable(BOARD_USART_DMA, BOARD_USART_DMA_TX_CH);
    dma_deinit(BOARD_USART_DMA, BOARD_USART_DMA_TX_CH);

    dma_single_data_para_struct_init(&dma_init_struct);
    dma_init_struct.direction = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.memory0_addr = (uint32_t)buf;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.number = len;
    dma_init_struct.periph_addr = BOARD_USART_DATA_ADDR;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(BOARD_USART_DMA, BOARD_USART_DMA_TX_CH, &dma_init_struct);

    dma_circulation_disable(BOARD_USART_DMA, BOARD_USART_DMA_TX_CH);
    dma_channel_subperipheral_select(BOARD_USART_DMA, BOARD_USART_DMA_TX_CH, BOARD_USART_DMA_SUBPERI);

    dma_flag_clear(BOARD_USART_DMA, BOARD_USART_DMA_TX_CH, DMA_FLAG_FTF);
    usart_dma_transmit_config(BOARD_USART, USART_TRANSMIT_DMA_ENABLE);
    dma_channel_enable(BOARD_USART_DMA, BOARD_USART_DMA_TX_CH);

    while(RESET == dma_flag_get(BOARD_USART_DMA, BOARD_USART_DMA_TX_CH, DMA_FLAG_FTF)) {
        if(0U == (--timeout)) {
            dma_channel_disable(BOARD_USART_DMA, BOARD_USART_DMA_TX_CH);
            usart_dma_transmit_config(BOARD_USART, USART_TRANSMIT_DMA_DISABLE);
            return 1U;
        }
    }

    timeout = USART_DMA_TIMEOUT;
    while(RESET == usart_flag_get(BOARD_USART, USART_FLAG_TC)) {
        if(0U == (--timeout)) {
            break;
        }
    }

    dma_channel_disable(BOARD_USART_DMA, BOARD_USART_DMA_TX_CH);
    usart_dma_transmit_config(BOARD_USART, USART_TRANSMIT_DMA_DISABLE);
    dma_flag_clear(BOARD_USART_DMA, BOARD_USART_DMA_TX_CH, DMA_FLAG_FTF);
    return 0U;
}

/*!
    \brief  RX DMA 已停用：CH2 划给 SPI0(Flash/LCD)
*/
uint8_t bsp_usart_dma_recv(uint8_t *buf, uint32_t len)
{
    (void)buf;
    (void)len;
    return 1U;
}

void bsp_usart_rx_irq_handler(void)
{
    if(RESET != usart_interrupt_flag_get(BOARD_USART, USART_INT_FLAG_RBNE)) {
        rx_push((uint8_t)usart_data_receive(BOARD_USART));
    }
    if(RESET != usart_interrupt_flag_get(BOARD_USART, USART_INT_FLAG_RBNE_ORERR)) {
        (void)usart_data_receive(BOARD_USART);
        usart_interrupt_flag_clear(BOARD_USART, USART_INT_FLAG_RBNE_ORERR);
    }
}

void bsp_usart_poll_rx(void)
{
    while(RESET != usart_flag_get(BOARD_USART, USART_FLAG_RBNE)) {
        rx_push((uint8_t)usart_data_receive(BOARD_USART));
    }
    if(RESET != usart_flag_get(BOARD_USART, USART_FLAG_ORERR)) {
        (void)usart_data_receive(BOARD_USART);
        usart_flag_clear(BOARD_USART, USART_FLAG_ORERR);
    }
}

uint8_t bsp_usart_getc(uint8_t *ch)
{
    if(NULL == ch) {
        return 0U;
    }
    if(s_rx_tail == s_rx_head) {
        return 0U;
    }
    *ch = s_rx_ring[s_rx_tail];
    s_rx_tail = (uint16_t)((s_rx_tail + 1U) % USART_RX_RING_SIZE);
    return 1U;
}

static void printf_flush_dma(void)
{
    if(0U == s_printf_len) {
        return;
    }
    (void)bsp_usart_dma_send(s_printf_buf, (uint32_t)s_printf_len);
    s_printf_len = 0U;
}

void bsp_usart_send_byte(uint8_t ch)
{
    /* 回显用：先排空 printf DMA，再轮询发；RX 靠中断不丢字节 */
    printf_flush_dma();
    usart_dma_transmit_config(BOARD_USART, USART_TRANSMIT_DMA_DISABLE);
    while(RESET == usart_flag_get(BOARD_USART, USART_FLAG_TBE)) {
    }
    usart_data_transmit(BOARD_USART, ch);
}

void bsp_usart_send_string(const char *s)
{
    uint32_t len;

    if(NULL == s) {
        return;
    }
    printf_flush_dma();
    len = (uint32_t)strlen(s);
    if(0U != len) {
        (void)bsp_usart_dma_send((const uint8_t *)s, len);
    }
}

static int usart_put_char(int ch)
{
    if(s_printf_len >= USART_PRINTF_BUF_SIZE) {
        printf_flush_dma();
    }
    s_printf_buf[s_printf_len++] = (uint8_t)ch;
    /* 遇换行或缓冲满：DMA 整包发出（官方 DMA 发送路径） */
    if(('\n' == ch) || (s_printf_len >= USART_PRINTF_BUF_SIZE)) {
        printf_flush_dma();
    }
    return ch;
}

#if defined(__GNUC__) && !defined(__clang__)
int __io_putchar(int ch)
{
    return usart_put_char(ch);
}
#else
int fputc(int ch, FILE *f)
{
    (void)f;
    return usart_put_char(ch);
}
#endif
