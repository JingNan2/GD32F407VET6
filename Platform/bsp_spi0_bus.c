/*!
    \file    bsp_spi0_bus.c
    \brief   SPI0 总线仲裁 + DMA 传输（Flash / LCD 分时）
*/

#include "bsp_spi0_bus.h"
#include "board.h"
#include "os.h"

#define DMA_TIMEOUT     0x20000U
#define SPI_PSC_FLASH   SPI_PSC_8   /* APB2=84M → ~10.5MHz */
#define SPI_PSC_LCD     SPI_PSC_2   /* APB2=84M → 42MHz（接近屏极限） */

static volatile spi0_bus_owner_t s_owner = SPI0_BUS_FREE;
static spi0_bus_owner_t s_spi_cfg = SPI0_BUS_FREE;
static volatile uint8_t s_flash_want = 0U;
static uint8_t s_dummy = 0xFFU;
static uint8_t s_tx_ff = 0xFFU;
static uint8_t s_inited = 0U;

static void spi_drain_rx(void)
{
    uint32_t guard = 64U;
    while((RESET != spi_i2s_flag_get(BOARD_SPI_FLASH, SPI_FLAG_RBNE)) && (0U != guard)) {
        (void)spi_i2s_data_receive(BOARD_SPI_FLASH);
        guard--;
    }
    /* 清 overrun：读 DR 再读 STAT */
    if(RESET != spi_i2s_flag_get(BOARD_SPI_FLASH, SPI_FLAG_RXORERR)) {
        (void)spi_i2s_data_receive(BOARD_SPI_FLASH);
        (void)SPI_STAT(BOARD_SPI_FLASH);
    }
}

static void spi_apply_owner(spi0_bus_owner_t owner)
{
    spi_parameter_struct spi_init_struct;

    gpio_bit_set(BOARD_SPI_FLASH_GPIO_PORT, BOARD_SPI_FLASH_CS_PIN);
    gpio_bit_set(BOARD_LCD_CS_PORT, BOARD_LCD_CS_PIN);

    if(s_spi_cfg == owner) {
        return;
    }

    spi_i2s_deinit(BOARD_SPI_FLASH);
    spi_struct_para_init(&spi_init_struct);
    spi_init_struct.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode          = SPI_MASTER;
    spi_init_struct.frame_size           = SPI_FRAMESIZE_8BIT;
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_LOW_PH_1EDGE;
    spi_init_struct.nss                  = SPI_NSS_SOFT;
    spi_init_struct.prescale             = (SPI0_BUS_LCD == owner) ? SPI_PSC_LCD : SPI_PSC_FLASH;
    spi_init_struct.endian               = SPI_ENDIAN_MSB;
    spi_init(BOARD_SPI_FLASH, &spi_init_struct);
    spi_enable(BOARD_SPI_FLASH);
    s_spi_cfg = owner;
}

void bsp_spi0_bus_init(void)
{
    if(0U != s_inited) {
        return;
    }

    rcu_periph_clock_enable(BOARD_SPI_FLASH_GPIO_CLK);
    rcu_periph_clock_enable(BOARD_SPI_FLASH_CLK);
    rcu_periph_clock_enable(BOARD_SPI_FLASH_DMA_CLK);
    rcu_periph_clock_enable(BOARD_LCD_CS_CLK);

    gpio_mode_set(BOARD_SPI_FLASH_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, BOARD_SPI_FLASH_CS_PIN);
    gpio_output_options_set(BOARD_SPI_FLASH_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, BOARD_SPI_FLASH_CS_PIN);
    gpio_bit_set(BOARD_SPI_FLASH_GPIO_PORT, BOARD_SPI_FLASH_CS_PIN);

    gpio_mode_set(BOARD_LCD_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, BOARD_LCD_CS_PIN);
    gpio_output_options_set(BOARD_LCD_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, BOARD_LCD_CS_PIN);
    gpio_bit_set(BOARD_LCD_CS_PORT, BOARD_LCD_CS_PIN);

    gpio_af_set(BOARD_SPI_FLASH_GPIO_PORT, BOARD_SPI_FLASH_AF,
                BOARD_SPI_FLASH_SCK_PIN | BOARD_SPI_FLASH_MISO_PIN | BOARD_SPI_FLASH_MOSI_PIN);
    gpio_mode_set(BOARD_SPI_FLASH_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE,
                  BOARD_SPI_FLASH_SCK_PIN | BOARD_SPI_FLASH_MISO_PIN | BOARD_SPI_FLASH_MOSI_PIN);
    gpio_output_options_set(BOARD_SPI_FLASH_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            BOARD_SPI_FLASH_SCK_PIN | BOARD_SPI_FLASH_MOSI_PIN);

    s_owner = SPI0_BUS_FREE;
    s_spi_cfg = SPI0_BUS_FREE;
    s_flash_want = 0U;
    spi_apply_owner(SPI0_BUS_FLASH);
    s_inited = 1U;
}

uint8_t bsp_spi0_bus_flash_pending(void)
{
    return s_flash_want;
}

uint8_t bsp_spi0_bus_acquire(spi0_bus_owner_t owner)
{
    if((SPI0_BUS_FLASH != owner) && (SPI0_BUS_LCD != owner)) {
        return SPI0_BUS_ERROR;
    }
    if(0U == s_inited) {
        bsp_spi0_bus_init();
    }

    if(SPI0_BUS_FLASH == owner) {
        s_flash_want = 1U;
        /* 调度器未跑时 os_yield 为空操作，必须带超时，否则会死等卡死 */
        {
            uint32_t spins = 0U;
            while(SPI0_BUS_FREE != s_owner) {
                os_yield();
                if(++spins > 2000000U) {
                    s_flash_want = 0U;
                    return SPI0_BUS_ERROR;
                }
            }
        }
        s_owner = SPI0_BUS_FLASH;
        s_flash_want = 0U;
    } else {
        {
            uint32_t spins = 0U;
            while((SPI0_BUS_FREE != s_owner) || (0U != s_flash_want)) {
                os_yield();
                if(++spins > 2000000U) {
                    return SPI0_BUS_ERROR;
                }
            }
        }
        s_owner = SPI0_BUS_LCD;
    }

    spi_apply_owner(owner);
    spi_drain_rx();
    return SPI0_BUS_OK;
}

void bsp_spi0_bus_release(spi0_bus_owner_t owner)
{
    if(s_owner == owner) {
        gpio_bit_set(BOARD_SPI_FLASH_GPIO_PORT, BOARD_SPI_FLASH_CS_PIN);
        gpio_bit_set(BOARD_LCD_CS_PORT, BOARD_LCD_CS_PIN);
        s_owner = SPI0_BUS_FREE;
    }
}

uint8_t bsp_spi0_bus_xfer_byte(uint8_t tx, uint8_t *rx)
{
    uint32_t spi = BOARD_SPI_FLASH;
    uint32_t timeout;
    uint8_t b;

    if(SPI0_BUS_FREE == s_owner) {
        return SPI0_BUS_ERROR;
    }

    timeout = DMA_TIMEOUT;
    while(RESET == spi_i2s_flag_get(spi, SPI_FLAG_TBE)) {
        if(0U == (--timeout)) {
            return SPI0_BUS_ERROR;
        }
    }
    spi_i2s_data_transmit(spi, (uint16_t)tx);

    timeout = DMA_TIMEOUT;
    while(RESET == spi_i2s_flag_get(spi, SPI_FLAG_RBNE)) {
        if(0U == (--timeout)) {
            return SPI0_BUS_ERROR;
        }
    }
    b = (uint8_t)spi_i2s_data_receive(spi);
    if(NULL != rx) {
        *rx = b;
    }
    return SPI0_BUS_OK;
}

uint8_t bsp_spi0_bus_write(const uint8_t *tx, uint32_t len)
{
    const uint8_t *p = tx;
    const uint8_t *end;
    uint32_t timeout;
    uint32_t spi = BOARD_SPI_FLASH;

    if(0U == len) {
        return SPI0_BUS_OK;
    }
    if((SPI0_BUS_FREE == s_owner) || (NULL == p)) {
        return SPI0_BUS_ERROR;
    }

    /* 紧循环喂 SPI：开销远小于每次 DMA deinit/重建 */
    end = p + len;
    while(p < end) {
        while(RESET == spi_i2s_flag_get(spi, SPI_FLAG_TBE)) {
        }
        spi_i2s_data_transmit(spi, (uint16_t)(*p++));
    }
    timeout = DMA_TIMEOUT;
    while(RESET != spi_i2s_flag_get(spi, SPI_FLAG_TRANS)) {
        if(0U == (--timeout)) {
            break;
        }
    }
    spi_drain_rx();
    return SPI0_BUS_OK;
}

uint8_t bsp_spi0_bus_dma_xfer(const uint8_t *tx, uint8_t *rx, uint32_t len)
{
    dma_single_data_parameter_struct dma_init_struct;
    uint32_t timeout;
    uint8_t *tx_ptr = (uint8_t *)tx;
    uint8_t use_dummy_tx = 0U;
    uint8_t use_dummy_rx = 0U;

    if(0U == len) {
        return SPI0_BUS_OK;
    }
    if(SPI0_BUS_FREE == s_owner) {
        return SPI0_BUS_ERROR;
    }
    /* GD32 DMA CNT 16bit */
    if(len > 0xFFFFU) {
        return SPI0_BUS_ERROR;
    }
    if(NULL == tx_ptr) {
        tx_ptr = &s_tx_ff;
        use_dummy_tx = 1U;
    }
    if(NULL == rx) {
        rx = &s_dummy;
        use_dummy_rx = 1U;
    }

    spi_dma_disable(BOARD_SPI_FLASH, SPI_DMA_TRANSMIT);
    spi_dma_disable(BOARD_SPI_FLASH, SPI_DMA_RECEIVE);
    dma_channel_disable(BOARD_SPI_FLASH_DMA, BOARD_SPI_FLASH_DMA_RX_CH);
    dma_channel_disable(BOARD_SPI_FLASH_DMA, BOARD_SPI_FLASH_DMA_TX_CH);

    dma_deinit(BOARD_SPI_FLASH_DMA, BOARD_SPI_FLASH_DMA_RX_CH);
    dma_deinit(BOARD_SPI_FLASH_DMA, BOARD_SPI_FLASH_DMA_TX_CH);

    /* RX：必须开，否则全双工 master 会卡 */
    dma_single_data_para_struct_init(&dma_init_struct);
    dma_init_struct.periph_addr = (uint32_t)&SPI_DATA(BOARD_SPI_FLASH);
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory0_addr = (uint32_t)rx;
    dma_init_struct.memory_inc = (0U != use_dummy_rx) ? DMA_MEMORY_INCREASE_DISABLE : DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.direction = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.number = len;
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(BOARD_SPI_FLASH_DMA, BOARD_SPI_FLASH_DMA_RX_CH, &dma_init_struct);
    dma_circulation_disable(BOARD_SPI_FLASH_DMA, BOARD_SPI_FLASH_DMA_RX_CH);
    dma_channel_subperipheral_select(BOARD_SPI_FLASH_DMA, BOARD_SPI_FLASH_DMA_RX_CH, BOARD_SPI_FLASH_DMA_SUBPERI);

    dma_single_data_para_struct_init(&dma_init_struct);
    dma_init_struct.periph_addr = (uint32_t)&SPI_DATA(BOARD_SPI_FLASH);
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory0_addr = (uint32_t)tx_ptr;
    dma_init_struct.memory_inc = (0U != use_dummy_tx) ? DMA_MEMORY_INCREASE_DISABLE : DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.direction = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.number = len;
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(BOARD_SPI_FLASH_DMA, BOARD_SPI_FLASH_DMA_TX_CH, &dma_init_struct);
    dma_circulation_disable(BOARD_SPI_FLASH_DMA, BOARD_SPI_FLASH_DMA_TX_CH);
    dma_channel_subperipheral_select(BOARD_SPI_FLASH_DMA, BOARD_SPI_FLASH_DMA_TX_CH, BOARD_SPI_FLASH_DMA_SUBPERI);

    dma_flag_clear(BOARD_SPI_FLASH_DMA, BOARD_SPI_FLASH_DMA_RX_CH, DMA_FLAG_FTF);
    dma_flag_clear(BOARD_SPI_FLASH_DMA, BOARD_SPI_FLASH_DMA_TX_CH, DMA_FLAG_FTF);
    dma_flag_clear(BOARD_SPI_FLASH_DMA, BOARD_SPI_FLASH_DMA_RX_CH, DMA_FLAG_TAE);
    dma_flag_clear(BOARD_SPI_FLASH_DMA, BOARD_SPI_FLASH_DMA_TX_CH, DMA_FLAG_TAE);

    spi_drain_rx();

    dma_channel_enable(BOARD_SPI_FLASH_DMA, BOARD_SPI_FLASH_DMA_RX_CH);
    dma_channel_enable(BOARD_SPI_FLASH_DMA, BOARD_SPI_FLASH_DMA_TX_CH);
    spi_dma_enable(BOARD_SPI_FLASH, SPI_DMA_RECEIVE);
    spi_dma_enable(BOARD_SPI_FLASH, SPI_DMA_TRANSMIT);

    /* 等 RX 完成（字节数与 TX 相同）；比只等 TX 更可靠 */
    timeout = DMA_TIMEOUT;
    while(RESET == dma_flag_get(BOARD_SPI_FLASH_DMA, BOARD_SPI_FLASH_DMA_RX_CH, DMA_FLAG_FTF)) {
        if(0U == (--timeout)) {
            goto fail;
        }
    }
    timeout = DMA_TIMEOUT;
    while(RESET != spi_i2s_flag_get(BOARD_SPI_FLASH, SPI_FLAG_TRANS)) {
        if(0U == (--timeout)) {
            break;
        }
    }

    spi_dma_disable(BOARD_SPI_FLASH, SPI_DMA_TRANSMIT);
    spi_dma_disable(BOARD_SPI_FLASH, SPI_DMA_RECEIVE);
    dma_channel_disable(BOARD_SPI_FLASH_DMA, BOARD_SPI_FLASH_DMA_TX_CH);
    dma_channel_disable(BOARD_SPI_FLASH_DMA, BOARD_SPI_FLASH_DMA_RX_CH);
    return SPI0_BUS_OK;

fail:
    spi_dma_disable(BOARD_SPI_FLASH, SPI_DMA_TRANSMIT);
    spi_dma_disable(BOARD_SPI_FLASH, SPI_DMA_RECEIVE);
    dma_channel_disable(BOARD_SPI_FLASH_DMA, BOARD_SPI_FLASH_DMA_TX_CH);
    dma_channel_disable(BOARD_SPI_FLASH_DMA, BOARD_SPI_FLASH_DMA_RX_CH);
    spi_drain_rx();
    return SPI0_BUS_ERROR;
}
