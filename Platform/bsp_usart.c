/*!
    \file    bsp_usart.c
    \brief   USART0 PA9/PA10 — Printf + DMA 发送；WriteFile 时 DMA 循环接收

    TX: DMA1 CH7 / SUBPERI4
    RX 普通：RBNE 中断 → 软件环缓
    RX WriteFile：DMA1 CH2 / SUBPERI4 循环 DMA（与 SPI0 Flash 分时，Flash 改轮询）
*/

#include "bsp_usart.h"
#include "board.h"
#include <stdio.h>
#include <string.h>

#define USART_PRINTF_BUF_SIZE  256U
#define USART_DMA_TIMEOUT      0x1FFFFFU
/* 115200 下扇区擦除可达数百 ms，必须能扛住突发；过小会导致 WriteFile 丢字节 */
#define USART_RX_RING_SIZE     8192U

static uint8_t  s_printf_buf[USART_PRINTF_BUF_SIZE];
static uint16_t s_printf_len = 0U;

static uint8_t  s_rx_ring[USART_RX_RING_SIZE];
static volatile uint16_t s_rx_head = 0U;
static volatile uint16_t s_rx_tail = 0U;

static volatile uint8_t s_rx_dma_on = 0U;
static volatile uint16_t s_rx_dma_tail = 0U;

static void rx_push(uint8_t ch)
{
    uint16_t next = (uint16_t)((s_rx_head + 1U) % USART_RX_RING_SIZE);
    if(next != s_rx_tail) {
        s_rx_ring[s_rx_head] = ch;
        s_rx_head = next;
    }
}

static uint16_t rx_dma_write_pos(void)
{
    uint32_t left;

    left = dma_transfer_number_get(BOARD_USART_DMA, BOARD_USART_DMA_RX_CH);
    if(left > USART_RX_RING_SIZE) {
        left = USART_RX_RING_SIZE;
    }
    return (uint16_t)(USART_RX_RING_SIZE - (uint16_t)left);
}

static uint32_t rx_dma_read(uint8_t *buf, uint32_t max_len)
{
    uint32_t got = 0U;
    uint16_t head;
    uint16_t tail;
    uint16_t chunk;

    if((NULL == buf) || (0U == max_len) || (0U == s_rx_dma_on)) {
        return 0U;
    }

    head = rx_dma_write_pos();
    tail = s_rx_dma_tail;

    while((got < max_len) && (tail != head)) {
        if(tail < head) {
            chunk = (uint16_t)(head - tail);
        } else {
            chunk = (uint16_t)(USART_RX_RING_SIZE - tail);
        }
        if(chunk > (uint16_t)(max_len - got)) {
            chunk = (uint16_t)(max_len - got);
        }
        (void)memcpy(buf + got, &s_rx_ring[tail], chunk);
        got += chunk;
        tail = (uint16_t)((tail + chunk) % USART_RX_RING_SIZE);
    }
    s_rx_dma_tail = tail;
    return got;
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
    s_rx_dma_on = 0U;
    s_rx_dma_tail = 0U;
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
    \brief  阻塞定长 RX DMA（仅非高速模式可用；高速模式请用 highspeed + read）
*/
uint8_t bsp_usart_dma_recv(uint8_t *buf, uint32_t len)
{
    dma_single_data_parameter_struct dma_init_struct;
    uint32_t timeout = USART_DMA_TIMEOUT;

    if((NULL == buf) || (0U == len) || (0U != s_rx_dma_on)) {
        return 1U;
    }

    usart_interrupt_disable(BOARD_USART, USART_INT_RBNE);

    dma_channel_disable(BOARD_USART_DMA, BOARD_USART_DMA_RX_CH);
    dma_deinit(BOARD_USART_DMA, BOARD_USART_DMA_RX_CH);

    dma_single_data_para_struct_init(&dma_init_struct);
    dma_init_struct.direction = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.memory0_addr = (uint32_t)buf;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.number = len;
    dma_init_struct.periph_addr = BOARD_USART_DATA_ADDR;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(BOARD_USART_DMA, BOARD_USART_DMA_RX_CH, &dma_init_struct);
    dma_circulation_disable(BOARD_USART_DMA, BOARD_USART_DMA_RX_CH);
    dma_channel_subperipheral_select(BOARD_USART_DMA, BOARD_USART_DMA_RX_CH, BOARD_USART_DMA_SUBPERI);

    dma_flag_clear(BOARD_USART_DMA, BOARD_USART_DMA_RX_CH, DMA_FLAG_FTF);
    usart_dma_receive_config(BOARD_USART, USART_RECEIVE_DMA_ENABLE);
    dma_channel_enable(BOARD_USART_DMA, BOARD_USART_DMA_RX_CH);

    while(RESET == dma_flag_get(BOARD_USART_DMA, BOARD_USART_DMA_RX_CH, DMA_FLAG_FTF)) {
        if(0U == (--timeout)) {
            dma_channel_disable(BOARD_USART_DMA, BOARD_USART_DMA_RX_CH);
            usart_dma_receive_config(BOARD_USART, USART_RECEIVE_DMA_DISABLE);
            usart_interrupt_enable(BOARD_USART, USART_INT_RBNE);
            return 1U;
        }
    }

    dma_channel_disable(BOARD_USART_DMA, BOARD_USART_DMA_RX_CH);
    usart_dma_receive_config(BOARD_USART, USART_RECEIVE_DMA_DISABLE);
    dma_flag_clear(BOARD_USART_DMA, BOARD_USART_DMA_RX_CH, DMA_FLAG_FTF);
    usart_interrupt_enable(BOARD_USART, USART_INT_RBNE);
    return 0U;
}

uint8_t bsp_usart_rx_dma_is_active(void)
{
    return s_rx_dma_on;
}

void bsp_usart_rx_dma_highspeed_enter(void)
{
    dma_single_data_parameter_struct dma_init_struct;

    if(0U != s_rx_dma_on) {
        return;
    }

    /* 先抽干 RBNE，再切 DMA，避免交接丢字节 */
    while(RESET != usart_flag_get(BOARD_USART, USART_FLAG_RBNE)) {
        rx_push((uint8_t)usart_data_receive(BOARD_USART));
    }
    if(RESET != usart_flag_get(BOARD_USART, USART_FLAG_ORERR)) {
        (void)usart_data_receive(BOARD_USART);
        usart_flag_clear(BOARD_USART, USART_FLAG_ORERR);
    }

    usart_interrupt_disable(BOARD_USART, USART_INT_RBNE);

    /* 软件环里未读完的字节保留；DMA 从空缓冲重新填 */
    s_rx_head = 0U;
    s_rx_tail = 0U;
    s_rx_dma_tail = 0U;
    (void)memset(s_rx_ring, 0, sizeof(s_rx_ring));

    dma_channel_disable(BOARD_USART_DMA, BOARD_USART_DMA_RX_CH);
    dma_deinit(BOARD_USART_DMA, BOARD_USART_DMA_RX_CH);

    dma_single_data_para_struct_init(&dma_init_struct);
    dma_init_struct.direction = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.memory0_addr = (uint32_t)s_rx_ring;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.number = USART_RX_RING_SIZE;
    dma_init_struct.periph_addr = BOARD_USART_DATA_ADDR;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(BOARD_USART_DMA, BOARD_USART_DMA_RX_CH, &dma_init_struct);
    dma_circulation_enable(BOARD_USART_DMA, BOARD_USART_DMA_RX_CH);
    dma_channel_subperipheral_select(BOARD_USART_DMA, BOARD_USART_DMA_RX_CH, BOARD_USART_DMA_SUBPERI);

    dma_flag_clear(BOARD_USART_DMA, BOARD_USART_DMA_RX_CH, DMA_FLAG_FTF);
    dma_flag_clear(BOARD_USART_DMA, BOARD_USART_DMA_RX_CH, DMA_FLAG_HTF);
    dma_flag_clear(BOARD_USART_DMA, BOARD_USART_DMA_RX_CH, DMA_FLAG_TAE);

    usart_dma_receive_config(BOARD_USART, USART_RECEIVE_DMA_ENABLE);
    dma_channel_enable(BOARD_USART_DMA, BOARD_USART_DMA_RX_CH);

    /* IDLE：线路空闲时清标志，便于上层判结束；数据仍由 DMA 搬 */
    usart_interrupt_flag_clear(BOARD_USART, USART_INT_FLAG_IDLE);
    usart_interrupt_enable(BOARD_USART, USART_INT_IDLE);

    s_rx_dma_on = 1U;
}

void bsp_usart_rx_dma_highspeed_exit(void)
{
    if(0U == s_rx_dma_on) {
        return;
    }

    usart_interrupt_disable(BOARD_USART, USART_INT_IDLE);

    /* 与软件环共用 s_rx_ring：停 DMA 后直接丢弃残留 BIN，避免污染命令行 */
    dma_channel_disable(BOARD_USART_DMA, BOARD_USART_DMA_RX_CH);
    usart_dma_receive_config(BOARD_USART, USART_RECEIVE_DMA_DISABLE);
    dma_deinit(BOARD_USART_DMA, BOARD_USART_DMA_RX_CH);

    if(RESET != usart_flag_get(BOARD_USART, USART_FLAG_ORERR)) {
        (void)usart_data_receive(BOARD_USART);
        usart_flag_clear(BOARD_USART, USART_FLAG_ORERR);
    }
    while(RESET != usart_flag_get(BOARD_USART, USART_FLAG_RBNE)) {
        (void)usart_data_receive(BOARD_USART);
    }

    s_rx_dma_on = 0U;
    s_rx_dma_tail = 0U;
    s_rx_head = 0U;
    s_rx_tail = 0U;
    usart_interrupt_enable(BOARD_USART, USART_INT_RBNE);
}

void bsp_usart_rx_irq_handler(void)
{
    if(0U != s_rx_dma_on) {
        if(RESET != usart_interrupt_flag_get(BOARD_USART, USART_INT_FLAG_IDLE)) {
            /* 读 DATA 清 IDLE（GD32：先读 STAT 再读 DATA） */
            (void)USART_STAT0(BOARD_USART);
            (void)usart_data_receive(BOARD_USART);
            usart_interrupt_flag_clear(BOARD_USART, USART_INT_FLAG_IDLE);
        }
        if(RESET != usart_interrupt_flag_get(BOARD_USART, USART_INT_FLAG_RBNE_ORERR)) {
            (void)usart_data_receive(BOARD_USART);
            usart_interrupt_flag_clear(BOARD_USART, USART_INT_FLAG_RBNE_ORERR);
        }
        return;
    }

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
    if(0U != s_rx_dma_on) {
        return;
    }
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
    if(0U != s_rx_dma_on) {
        return (1U == rx_dma_read(ch, 1U)) ? 1U : 0U;
    }
    if(s_rx_tail == s_rx_head) {
        return 0U;
    }
    *ch = s_rx_ring[s_rx_tail];
    s_rx_tail = (uint16_t)((s_rx_tail + 1U) % USART_RX_RING_SIZE);
    return 1U;
}

uint32_t bsp_usart_read(uint8_t *buf, uint32_t max_len)
{
    uint32_t got = 0U;

    if((NULL == buf) || (0U == max_len)) {
        return 0U;
    }

    if(0U != s_rx_dma_on) {
        return rx_dma_read(buf, max_len);
    }

    while((got < max_len) && (s_rx_tail != s_rx_head)) {
        buf[got++] = s_rx_ring[s_rx_tail];
        s_rx_tail = (uint16_t)((s_rx_tail + 1U) % USART_RX_RING_SIZE);
    }
    return got;
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
    /* 回显用：先排空 printf DMA，再轮询发；RX 靠中断/DMA 不丢字节 */
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
