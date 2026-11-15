#ifndef BSP_SPI_FLASH_H
#define BSP_SPI_FLASH_H

#include <stdint.h>

/* ZD25Q32D：32Mbit = 4MB，页 256B，扇区 4KB，块 64KB */
#define SPI_FLASH_PAGE_SIZE       256U
#define SPI_FLASH_SECTOR_SIZE     4096U
#define SPI_FLASH_BLOCK_SIZE      65536U
#define SPI_FLASH_SIZE            (4U * 1024U * 1024U)

#define SPI_FLASH_OK              0U
#define SPI_FLASH_ERROR           1U

uint8_t  bsp_spi_flash_init(void);
uint8_t  bsp_spi_flash_read_id(uint8_t *mid, uint8_t *mem_type, uint8_t *capacity);
uint8_t  bsp_spi_flash_read(uint32_t addr, uint8_t *buf, uint32_t len);
uint8_t  bsp_spi_flash_page_program(uint32_t addr, const uint8_t *buf, uint32_t len);
uint8_t  bsp_spi_flash_write(uint32_t addr, const uint8_t *buf, uint32_t len);
uint8_t  bsp_spi_flash_sector_erase(uint32_t addr);
uint8_t  bsp_spi_flash_block_erase(uint32_t addr);
uint8_t  bsp_spi_flash_chip_erase(void);
uint8_t  bsp_spi_flash_wait_busy(void);

#endif /* BSP_SPI_FLASH_H */
