/*!
    \file    bsp_spi_flash.c
    \brief   ZD25Q32D SPI Flash（指令兼容 W25Q32）
             命令/状态用轮询 SPI；大数据才走 DMA
*/

#include "bsp_spi_flash.h"
#include "bsp_spi0_bus.h"
#include "board.h"
#include "systick.h"
#include <stdio.h>

#define CMD_WREN          0x06U
#define CMD_WRDI          0x04U
#define CMD_RDSR1         0x05U
#define CMD_WRSR          0x01U
#define CMD_READ          0x03U
#define CMD_PAGE_PROG     0x02U
#define CMD_SECTOR_ERASE  0x20U
#define CMD_BLOCK_ERASE   0xD8U
#define CMD_CHIP_ERASE    0xC7U
#define CMD_RDID          0x9FU

#define SR_BUSY           0x01U
#define SR_WEL            0x02U

#define FLASH_BUSY_TO_MS  5000U   /* 扇区擦除一般 <400ms */
#define FLASH_CHIP_TO_MS  120000U

static uint8_t s_ready = 0U;

static void cs_low(void)
{
    gpio_bit_set(BOARD_LCD_CS_PORT, BOARD_LCD_CS_PIN);
    gpio_bit_reset(BOARD_SPI_FLASH_GPIO_PORT, BOARD_SPI_FLASH_CS_PIN);
}

static void cs_high(void)
{
    gpio_bit_set(BOARD_SPI_FLASH_GPIO_PORT, BOARD_SPI_FLASH_CS_PIN);
}

static uint8_t flash_begin(void)
{
    return bsp_spi0_bus_acquire(SPI0_BUS_FLASH);
}

static void flash_end(void)
{
    cs_high();
    bsp_spi0_bus_release(SPI0_BUS_FLASH);
}

static uint8_t flash_tx(uint8_t b)
{
    return bsp_spi0_bus_xfer_byte(b, NULL);
}

static uint8_t flash_txrx(uint8_t b, uint8_t *rx)
{
    return bsp_spi0_bus_xfer_byte(b, rx);
}

static uint8_t flash_write_enable(void)
{
    cs_high();
    cs_low();
    if(SPI0_BUS_OK != flash_tx(CMD_WREN)) {
        cs_high();
        return SPI_FLASH_ERROR;
    }
    cs_high();
    return SPI_FLASH_OK;
}

static uint8_t flash_read_sr1(uint8_t *sr)
{
    uint8_t v = 0xFFU;

    cs_low();
    if(SPI0_BUS_OK != flash_tx(CMD_RDSR1)) {
        cs_high();
        return SPI_FLASH_ERROR;
    }
    if(SPI0_BUS_OK != flash_txrx(0xFFU, &v)) {
        cs_high();
        return SPI_FLASH_ERROR;
    }
    cs_high();
    *sr = v;
    return SPI_FLASH_OK;
}

/*! 调用方已持有总线；擦除命令后 CS 已拉高 */
static uint8_t flash_wait_busy_held(uint32_t timeout_ms)
{
    uint32_t t0 = sys_tick_get();
    uint8_t sr;

    for(;;) {
        if(SPI_FLASH_OK != flash_read_sr1(&sr)) {
            return SPI_FLASH_ERROR;
        }
        if(0U == (sr & SR_BUSY)) {
            return SPI_FLASH_OK;
        }
        if(0U != sys_tick_expired(t0, timeout_ms)) {
            printf("[FLASH] busy timeout sr=0x%02X\r\n", sr);
            return SPI_FLASH_ERROR;
        }
    }
}

static uint8_t flash_wait_busy(uint32_t timeout_ms)
{
    if(SPI0_BUS_OK != flash_begin()) {
        return SPI_FLASH_ERROR;
    }
    if(SPI_FLASH_OK != flash_wait_busy_held(timeout_ms)) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    flash_end();
    return SPI_FLASH_OK;
}

/* 解除块保护，避免扇区擦除被状态寄存器拦住 */
static uint8_t flash_clear_protect(void)
{
    uint8_t sr = 0U;

    if(SPI_FLASH_OK != flash_write_enable()) {
        return SPI_FLASH_ERROR;
    }
    cs_low();
    if(SPI0_BUS_OK != flash_tx(CMD_WRSR)) {
        cs_high();
        return SPI_FLASH_ERROR;
    }
    if(SPI0_BUS_OK != flash_tx(0x00U)) { /* 清 BP/SRP */
        cs_high();
        return SPI_FLASH_ERROR;
    }
    cs_high();
    if(SPI_FLASH_OK != flash_wait_busy_held(FLASH_BUSY_TO_MS)) {
        return SPI_FLASH_ERROR;
    }
    if(SPI_FLASH_OK != flash_read_sr1(&sr)) {
        return SPI_FLASH_ERROR;
    }
    printf("[FLASH] SR1=0x%02X after unprotect\r\n", sr);
    return SPI_FLASH_OK;
}

uint8_t bsp_spi_flash_wait_busy(void)
{
    return flash_wait_busy(FLASH_BUSY_TO_MS);
}

uint8_t bsp_spi_flash_init(void)
{
    uint8_t mid = 0U, mtype = 0U, cap = 0U;

    s_ready = 0U;
    bsp_spi0_bus_init();

    if(SPI_FLASH_OK != bsp_spi_flash_read_id(&mid, &mtype, &cap)) {
        return SPI_FLASH_ERROR;
    }
    if((0U == mid) || (0xFFU == mid)) {
        return SPI_FLASH_ERROR;
    }

    if(SPI0_BUS_OK != flash_begin()) {
        return SPI_FLASH_ERROR;
    }
    (void)flash_clear_protect();
    flash_end();

    s_ready = 1U;
    return SPI_FLASH_OK;
}

uint8_t bsp_spi_flash_read_id(uint8_t *mid, uint8_t *mem_type, uint8_t *capacity)
{
    uint8_t id[3] = {0};

    if(SPI0_BUS_OK != flash_begin()) {
        return SPI_FLASH_ERROR;
    }
    cs_low();
    if(SPI0_BUS_OK != flash_tx(CMD_RDID) ||
       SPI0_BUS_OK != flash_txrx(0xFFU, &id[0]) ||
       SPI0_BUS_OK != flash_txrx(0xFFU, &id[1]) ||
       SPI0_BUS_OK != flash_txrx(0xFFU, &id[2])) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    flash_end();

    if(NULL != mid) {
        *mid = id[0];
    }
    if(NULL != mem_type) {
        *mem_type = id[1];
    }
    if(NULL != capacity) {
        *capacity = id[2];
    }
    return SPI_FLASH_OK;
}

uint8_t bsp_spi_flash_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    if((0U == s_ready) || (NULL == buf) || (0U == len) || ((addr + len) > SPI_FLASH_SIZE)) {
        return SPI_FLASH_ERROR;
    }

    if(SPI0_BUS_OK != flash_begin()) {
        return SPI_FLASH_ERROR;
    }
    cs_low();
    if(SPI0_BUS_OK != flash_tx(CMD_READ) ||
       SPI0_BUS_OK != flash_tx((uint8_t)(addr >> 16)) ||
       SPI0_BUS_OK != flash_tx((uint8_t)(addr >> 8)) ||
       SPI0_BUS_OK != flash_tx((uint8_t)addr)) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    /* 大数据用 DMA */
    if(SPI0_BUS_OK != bsp_spi0_bus_dma_xfer(NULL, buf, len)) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    flash_end();
    return SPI_FLASH_OK;
}

uint8_t bsp_spi_flash_page_program(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint8_t sr = 0U;

    if((0U == s_ready) || (NULL == buf) || (0U == len) || (len > SPI_FLASH_PAGE_SIZE) ||
       ((addr + len) > SPI_FLASH_SIZE) ||
       (((addr & (SPI_FLASH_PAGE_SIZE - 1U)) + len) > SPI_FLASH_PAGE_SIZE)) {
        return SPI_FLASH_ERROR;
    }

    if(SPI0_BUS_OK != flash_begin()) {
        return SPI_FLASH_ERROR;
    }
    if(SPI_FLASH_OK != flash_write_enable()) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    if(SPI_FLASH_OK != flash_read_sr1(&sr) || (0U == (sr & SR_WEL))) {
        printf("[FLASH] WEL not set sr=0x%02X\r\n", sr);
        flash_end();
        return SPI_FLASH_ERROR;
    }

    cs_low();
    if(SPI0_BUS_OK != flash_tx(CMD_PAGE_PROG) ||
       SPI0_BUS_OK != flash_tx((uint8_t)(addr >> 16)) ||
       SPI0_BUS_OK != flash_tx((uint8_t)(addr >> 8)) ||
       SPI0_BUS_OK != flash_tx((uint8_t)addr)) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    if(SPI0_BUS_OK != bsp_spi0_bus_dma_xfer(buf, NULL, len)) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    cs_high();
    if(SPI_FLASH_OK != flash_wait_busy_held(FLASH_BUSY_TO_MS)) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    flash_end();
    return SPI_FLASH_OK;
}

uint8_t bsp_spi_flash_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t page_remain;
    uint32_t chunk;

    if((0U == s_ready) || (NULL == buf) || (0U == len) || ((addr + len) > SPI_FLASH_SIZE)) {
        return SPI_FLASH_ERROR;
    }

    while(0U != len) {
        page_remain = SPI_FLASH_PAGE_SIZE - (addr & (SPI_FLASH_PAGE_SIZE - 1U));
        chunk = (len < page_remain) ? len : page_remain;
        if(SPI_FLASH_OK != bsp_spi_flash_page_program(addr, buf, chunk)) {
            return SPI_FLASH_ERROR;
        }
        addr += chunk;
        buf += chunk;
        len -= chunk;
    }
    return SPI_FLASH_OK;
}

uint8_t bsp_spi_flash_sector_erase(uint32_t addr)
{
    uint8_t sr = 0U;

    if((0U == s_ready) || (addr >= SPI_FLASH_SIZE)) {
        return SPI_FLASH_ERROR;
    }
    addr &= ~(SPI_FLASH_SECTOR_SIZE - 1U);

    if(SPI0_BUS_OK != flash_begin()) {
        printf("[FLASH] erase acquire fail\r\n");
        return SPI_FLASH_ERROR;
    }
    if(SPI_FLASH_OK != flash_write_enable()) {
        printf("[FLASH] erase WREN fail\r\n");
        flash_end();
        return SPI_FLASH_ERROR;
    }
    if(SPI_FLASH_OK != flash_read_sr1(&sr) || (0U == (sr & SR_WEL))) {
        printf("[FLASH] erase WEL fail sr=0x%02X\r\n", sr);
        flash_end();
        return SPI_FLASH_ERROR;
    }

    cs_low();
    if(SPI0_BUS_OK != flash_tx(CMD_SECTOR_ERASE) ||
       SPI0_BUS_OK != flash_tx((uint8_t)(addr >> 16)) ||
       SPI0_BUS_OK != flash_tx((uint8_t)(addr >> 8)) ||
       SPI0_BUS_OK != flash_tx((uint8_t)addr)) {
        printf("[FLASH] erase cmd fail\r\n");
        flash_end();
        return SPI_FLASH_ERROR;
    }
    cs_high(); /* 拉高 CS 后芯片开始擦 */

    if(SPI_FLASH_OK != flash_wait_busy_held(FLASH_BUSY_TO_MS)) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    flash_end();
    return SPI_FLASH_OK;
}

uint8_t bsp_spi_flash_block_erase(uint32_t addr)
{
    if((0U == s_ready) || (addr >= SPI_FLASH_SIZE)) {
        return SPI_FLASH_ERROR;
    }
    addr &= ~(SPI_FLASH_BLOCK_SIZE - 1U);

    if(SPI0_BUS_OK != flash_begin()) {
        return SPI_FLASH_ERROR;
    }
    if(SPI_FLASH_OK != flash_write_enable()) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    cs_low();
    if(SPI0_BUS_OK != flash_tx(CMD_BLOCK_ERASE) ||
       SPI0_BUS_OK != flash_tx((uint8_t)(addr >> 16)) ||
       SPI0_BUS_OK != flash_tx((uint8_t)(addr >> 8)) ||
       SPI0_BUS_OK != flash_tx((uint8_t)addr)) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    cs_high();
    if(SPI_FLASH_OK != flash_wait_busy_held(FLASH_BUSY_TO_MS)) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    flash_end();
    return SPI_FLASH_OK;
}

uint8_t bsp_spi_flash_chip_erase(void)
{
    if(0U == s_ready) {
        return SPI_FLASH_ERROR;
    }
    if(SPI0_BUS_OK != flash_begin()) {
        return SPI_FLASH_ERROR;
    }
    if(SPI_FLASH_OK != flash_write_enable()) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    cs_low();
    if(SPI0_BUS_OK != flash_tx(CMD_CHIP_ERASE)) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    cs_high();
    if(SPI_FLASH_OK != flash_wait_busy_held(FLASH_CHIP_TO_MS)) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    flash_end();
    return SPI_FLASH_OK;
}
