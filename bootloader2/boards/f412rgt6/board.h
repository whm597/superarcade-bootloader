#ifndef BOARD_H
#define BOARD_H

#define OSC_FREQ 8
#define USBDEVICESTRING "Arcade F412RGT6"
#define USBMFGSTRING "Contoso Ltd."
#define BOARD_FLASH_SECTORS 11
#define BOARD_FLASH_SIZE (1024 * 1024)

#define BL_F412 1

#ifdef DEFINE_CONFIGDATA
__attribute__((section(".config"))) __attribute__((used)) //
const uint32_t configData[] = {
    /* CF2 START */
    513675505, 539130489, // magic
    35, 100,  // used entries, total entries
    4, 0x28, // PIN_BTN_A = PC08
    5, 0x06, // PIN_BTN_B = PA06
    21, 0x0a, // PIN_RX = PA10 (DEBUG UART USART1 RX)
    28, 0x09, // PIN_TX = PA09 (DEBUG UART USART1 TX)
    33, 0x25, // PIN_DISPLAY_MISO = PC05 (LCD RD / NOE)
    34, 0x22, // PIN_DISPLAY_MOSI = PC02 (LCD WR / NWE)
    35, 0x24, // PIN_DISPLAY_CS = PC04
    36, 0x23, // PIN_DISPLAY_DC = PC03 (LCD RS / A0)
    37, 0x140, // DISPLAY_WIDTH = 320
    38, 0xf0, // DISPLAY_HEIGHT = 240
    39, 0x8, // DISPLAY_CFG0 = 0x8
    40, 0x10ff, // DISPLAY_CFG1 = 0x10ff
    41, 0x1000004, // DISPLAY_CFG2 = 0x1000004
    43, 0x27, // PIN_DISPLAY_RST = PC07
    44, 0x19, // PIN_DISPLAY_BL = PB09
    47, 0x00, // PIN_BTN_LEFT = PA00
    48, 0x18, // PIN_BTN_RIGHT = PB08
    49, 0x01, // PIN_BTN_UP = PA01
    50, 0x17, // PIN_BTN_DOWN = PB07
    51, 0x21, // PIN_BTN_MENU = PC01
    60, 0x1a, // PIN_JACK_TX = PB10 (Jacdac DAT)
    65, 0x08, // PIN_JACK_SND = PA08 (audio PWM, TIM1_CH1)
    74, 0x20, // PIN_BTN_MENU2 = PC00 (SEL)
    75, 0x10, // PIN_BATTSENSE = PB00 (100K/100K divider)
    77, 0x2d, // PIN_PWREN = PC13 (5V output, high=on)
    78, 0x247d, // DISPLAY_TYPE = 9341 (ILI9341)
    151, 0x29, // PIN_D1 = PC09 (X button)
    152, 0x11, // PIN_D2 = PB01 (Y button)
    160, 0x0f, // PIN_SPI_CS = PA15 (W25Q16 CS)
    161, 0x13, // PIN_SPI_CLK = PB03 (W25Q16 SCLK)
    162, 0x15, // PIN_SPI_MOSI = PB05 (W25Q16 MOSI)
    163, 0x14, // PIN_SPI_MISO = PB04 (W25Q16 MISO)
    204, 0x100000, // FLASH_BYTES = 1MB
    205, 0x40000, // RAM_BYTES = 256KB
    208, 0x2a0c7190, // BOOTLOADER_BOARD_ID = 0x2a0c7190
    209, 0x57755a57, // UF2_FAMILY = Arcade
    210, 0x10, // PINS_PORT_SIZE = PA_16
    2102, 0x140, // CFG_ARCADE_SCREEN_WIDTH = 320
    2103, 0xf0, // CFG_ARCADE_SCREEN_HEIGHT = 240
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* CF2 END */
};
#endif

#endif /* BOARD_H */
