/*!
    \file    bsp_spi_flash.c
    \brief   ZD25Q32D SPI Flash（指令兼容 W25Q32）
             经 bsp_spi0_bus 与 LCD 分时共用 SPI0 DMA（Flash 优先）
*/

#include "bsp_spi_flash.h"
#include "bsp_spi0_bus.h"
#include "board.h"

#define CMD_WREN          0x06U
#define CMD_RDSR1         0x05U
#define CMD_READ          0x03U
#define CMD_PAGE_PROG     0x02U
#define CMD_SECTOR_ERASE  0x20U
#define CMD_BLOCK_ERASE   0xD8U
#define CMD_CHIP_ERASE    0xC7U
#define CMD_RDID          0x9FU
#define SR_BUSY           0x01U

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

static uint8_t spi_dma_xfer(const uint8_t *tx, uint8_t *rx, uint32_t len)
{
    return (SPI0_BUS_OK == bsp_spi0_bus_dma_xfer(tx, rx, len)) ? SPI_FLASH_OK : SPI_FLASH_ERROR;
}

static uint8_t spi_write_read_byte(uint8_t data)
{
    uint8_t rx = 0U;
    (void)spi_dma_xfer(&data, &rx, 1U);
    return rx;
}

static uint8_t flash_write_enable(void)
{
    uint8_t cmd = CMD_WREN;
    cs_low();
    if(SPI_FLASH_OK != spi_dma_xfer(&cmd, NULL, 1U)) {
        cs_high();
        return SPI_FLASH_ERROR;
    }
    cs_high();
    return SPI_FLASH_OK;
}

uint8_t bsp_spi_flash_wait_busy(void)
{
    uint8_t sr;
    uint8_t cmd = CMD_RDSR1;
    uint32_t timeout = 0xFFFFFFU;

    if(SPI0_BUS_OK != flash_begin()) {
        return SPI_FLASH_ERROR;
    }

    do {
        cs_low();
        (void)spi_dma_xfer(&cmd, NULL, 1U);
        sr = spi_write_read_byte(0xFFU);
        cs_high();
        if(0U == (sr & SR_BUSY)) {
            flash_end();
            return SPI_FLASH_OK;
        }
    } while(--timeout);

    flash_end();
    return SPI_FLASH_ERROR;
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

    s_ready = 1U;
    return SPI_FLASH_OK;
}

uint8_t bsp_spi_flash_read_id(uint8_t *mid, uint8_t *mem_type, uint8_t *capacity)
{
    uint8_t cmd = CMD_RDID;
    uint8_t id[3] = {0};

    if(SPI0_BUS_OK != flash_begin()) {
        return SPI_FLASH_ERROR;
    }
    cs_low();
    if(SPI_FLASH_OK != spi_dma_xfer(&cmd, NULL, 1U)) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    if(SPI_FLASH_OK != spi_dma_xfer(NULL, id, 3U)) {
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
    uint8_t hdr[4];

    if((0U == s_ready) || (NULL == buf) || (0U == len) || ((addr + len) > SPI_FLASH_SIZE)) {
        return SPI_FLASH_ERROR;
    }

    hdr[0] = CMD_READ;
    hdr[1] = (uint8_t)(addr >> 16);
    hdr[2] = (uint8_t)(addr >> 8);
    hdr[3] = (uint8_t)(addr);

    if(SPI0_BUS_OK != flash_begin()) {
        return SPI_FLASH_ERROR;
    }
    cs_low();
    if(SPI_FLASH_OK != spi_dma_xfer(hdr, NULL, 4U)) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    if(SPI_FLASH_OK != spi_dma_xfer(NULL, buf, len)) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    flash_end();
    return SPI_FLASH_OK;
}

uint8_t bsp_spi_flash_page_program(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint8_t hdr[4];

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

    hdr[0] = CMD_PAGE_PROG;
    hdr[1] = (uint8_t)(addr >> 16);
    hdr[2] = (uint8_t)(addr >> 8);
    hdr[3] = (uint8_t)(addr);

    cs_low();
    if(SPI_FLASH_OK != spi_dma_xfer(hdr, NULL, 4U)) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    if(SPI_FLASH_OK != spi_dma_xfer(buf, NULL, len)) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    cs_high();
    /* wait_busy 会再次 acquire —— 先 release */
    flash_end();
    return bsp_spi_flash_wait_busy();
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
    uint8_t hdr[4];

    if((0U == s_ready) || (addr >= SPI_FLASH_SIZE)) {
        return SPI_FLASH_ERROR;
    }
    addr &= ~(SPI_FLASH_SECTOR_SIZE - 1U);

    if(SPI0_BUS_OK != flash_begin()) {
        return SPI_FLASH_ERROR;
    }
    if(SPI_FLASH_OK != flash_write_enable()) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    hdr[0] = CMD_SECTOR_ERASE;
    hdr[1] = (uint8_t)(addr >> 16);
    hdr[2] = (uint8_t)(addr >> 8);
    hdr[3] = (uint8_t)(addr);

    cs_low();
    if(SPI_FLASH_OK != spi_dma_xfer(hdr, NULL, 4U)) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    flash_end();
    return bsp_spi_flash_wait_busy();
}

uint8_t bsp_spi_flash_block_erase(uint32_t addr)
{
    uint8_t hdr[4];

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
    hdr[0] = CMD_BLOCK_ERASE;
    hdr[1] = (uint8_t)(addr >> 16);
    hdr[2] = (uint8_t)(addr >> 8);
    hdr[3] = (uint8_t)(addr);

    cs_low();
    if(SPI_FLASH_OK != spi_dma_xfer(hdr, NULL, 4U)) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    flash_end();
    return bsp_spi_flash_wait_busy();
}

uint8_t bsp_spi_flash_chip_erase(void)
{
    uint8_t cmd = CMD_CHIP_ERASE;

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
    if(SPI_FLASH_OK != spi_dma_xfer(&cmd, NULL, 1U)) {
        flash_end();
        return SPI_FLASH_ERROR;
    }
    flash_end();
    return bsp_spi_flash_wait_busy();
}
