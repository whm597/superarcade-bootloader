/**
 * W25Q16 SPI Flash 软件驱动 (bootloader)
 * 引脚: CS=PA15, CLK=PB3, MOSI=PB5, MISO=PB4
 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <stdint.h>

#define SPI_CS_PORT   GPIOA
#define SPI_CS_PIN    GPIO15
#define SPI_CLK_PORT  GPIOB
#define SPI_CLK_PIN   GPIO3
#define SPI_MOSI_PORT GPIOB
#define SPI_MOSI_PIN  GPIO5
#define SPI_MISO_PORT GPIOB
#define SPI_MISO_PIN  GPIO4

static void spi_cs_low(void)  { gpio_clear(SPI_CS_PORT, SPI_CS_PIN); }
static void spi_cs_high(void) { gpio_set(SPI_CS_PORT, SPI_CS_PIN); }
static void spi_clk_low(void) { gpio_clear(SPI_CLK_PORT, SPI_CLK_PIN); }
static void spi_clk_high(void){ gpio_set(SPI_CLK_PORT, SPI_CLK_PIN); }
static void spi_mosi_low(void) { gpio_clear(SPI_MOSI_PORT, SPI_MOSI_PIN); }
static void spi_mosi_high(void){ gpio_set(SPI_MOSI_PORT, SPI_MOSI_PIN); }
static int  spi_miso_read(void) { return gpio_get(SPI_MISO_PORT, SPI_MISO_PIN) ? 1 : 0; }

static void spi_gpio_init(void) {
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);

    /* CS, CLK, MOSI = output */
    gpio_mode_setup(SPI_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPI_CS_PIN);
    gpio_mode_setup(SPI_CLK_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPI_CLK_PIN);
    gpio_mode_setup(SPI_MOSI_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPI_MOSI_PIN);
    /* MISO = input */
    gpio_mode_setup(SPI_MISO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, SPI_MISO_PIN);

    spi_cs_high();
    spi_clk_low();
    spi_mosi_low();
}

static void spi_write_byte(uint8_t val) {
    for (int i = 7; i >= 0; i--) {
        if (val & (1 << i))
            spi_mosi_high();
        else
            spi_mosi_low();
        spi_clk_high();
        spi_clk_low();
    }
}

static uint8_t spi_read_byte(void) {
    uint8_t val = 0;
    for (int i = 7; i >= 0; i--) {
        spi_clk_high();
        if (spi_miso_read())
            val |= (1 << i);
        spi_clk_low();
    }
    return val;
}

/* ---------- public API ---------- */

int spi_flash_detect(void) {
    spi_gpio_init();
    spi_cs_low();
    spi_write_byte(0x9F);  /* Read JEDEC ID */
    uint8_t mfr = spi_read_byte();
    uint8_t type = spi_read_byte();
    uint8_t cap = spi_read_byte();
    spi_cs_high();
    /* W25Q16: manufacturer=0xEF, type=0x40, capacity=0x15 */
    return (mfr == 0xEF && type == 0x40) ? 1 : 0;
}

void spi_flash_read(uint32_t addr, uint8_t *buf, uint32_t len) {
    spi_cs_low();
    spi_write_byte(0x03);
    spi_write_byte((addr >> 16) & 0xFF);
    spi_write_byte((addr >> 8) & 0xFF);
    spi_write_byte(addr & 0xFF);
    for (uint32_t i = 0; i < len; i++)
        buf[i] = spi_read_byte();
    spi_cs_high();
}

static void spi_flash_wait_busy(void) {
    spi_cs_low();
    spi_write_byte(0x05);  /* Read Status Register */
    while (spi_read_byte() & 0x01)
        ;
    spi_cs_high();
}

static void spi_flash_write_enable(void) {
    spi_cs_low();
    spi_write_byte(0x06);
    spi_cs_high();
}

void spi_flash_erase_sector(uint32_t addr) {
    spi_flash_wait_busy();
    spi_flash_write_enable();
    spi_cs_low();
    spi_write_byte(0x20);  /* Sector Erase 4KB */
    spi_write_byte((addr >> 16) & 0xFF);
    spi_write_byte((addr >> 8) & 0xFF);
    spi_write_byte(addr & 0xFF);
    spi_cs_high();
    spi_flash_wait_busy();
}

void spi_flash_write(uint32_t addr, const uint8_t *data, uint32_t len) {
    uint32_t offset = 0;
    while (offset < len) {
        uint32_t chunk = 256 - (addr & 0xFF);  /* page boundary */
        if (chunk > len - offset)
            chunk = len - offset;
        spi_flash_wait_busy();
        spi_flash_write_enable();
        spi_cs_low();
        spi_write_byte(0x02);  /* Page Program */
        spi_write_byte((addr >> 16) & 0xFF);
        spi_write_byte((addr >> 8) & 0xFF);
        spi_write_byte(addr & 0xFF);
        for (uint32_t i = 0; i < chunk; i++)
            spi_write_byte(data[offset + i]);
        spi_cs_high();
        offset += chunk;
        addr += chunk;
    }
    spi_flash_wait_busy();
}
