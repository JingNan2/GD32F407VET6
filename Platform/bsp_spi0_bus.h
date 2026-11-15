#ifndef BSP_SPI0_BUS_H
#define BSP_SPI0_BUS_H

#include <stdint.h>

/* SPI0 总线分时：Flash / LCD 共用 DMA1 CH2(RX)+CH5(TX)
 * 优先级：Flash > LCD（LCD 持有时若 Flash 请求，LCD 在行间让出）
 */
typedef enum {
    SPI0_BUS_FREE  = 0,
    SPI0_BUS_FLASH = 1,
    SPI0_BUS_LCD   = 2
} spi0_bus_owner_t;

#define SPI0_BUS_OK     0U
#define SPI0_BUS_ERROR  1U

void    bsp_spi0_bus_init(void);
uint8_t bsp_spi0_bus_acquire(spi0_bus_owner_t owner);
void    bsp_spi0_bus_release(spi0_bus_owner_t owner);
uint8_t bsp_spi0_bus_dma_xfer(const uint8_t *tx, uint8_t *rx, uint32_t len);
/*! LCD 专用高速写：CPU 喂 SPI，无 DMA 重建开销，可达时钟上限 */
uint8_t bsp_spi0_bus_write(const uint8_t *tx, uint32_t len);
/*! 全双工轮询收发 1 字节（Flash 命令/读状态更稳） */
uint8_t bsp_spi0_bus_xfer_byte(uint8_t tx, uint8_t *rx);
uint8_t bsp_spi0_bus_flash_pending(void);

#endif /* BSP_SPI0_BUS_H */
