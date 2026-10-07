#pragma once
#include <stdint.h>

int spi_flash_detect(void);
void spi_flash_read(uint32_t addr, uint8_t *buf, uint32_t len);
void spi_flash_erase_sector(uint32_t addr);
void spi_flash_write(uint32_t addr, const uint8_t *data, uint32_t len);
