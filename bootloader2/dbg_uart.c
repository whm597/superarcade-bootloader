/* Minimal USART1 (PA9) 115200 debug trace */
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>

static int uart_inited = 0;

void dbg_uart_init(void) {
    if (uart_inited) return;
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_USART1);

    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9);
    gpio_set_af(GPIOA, GPIO_AF7, GPIO9);

    usart_set_baudrate(USART1, 115200);
    usart_set_databits(USART1, 8);
    usart_set_stopbits(USART1, USART_STOPBITS_1);
    usart_set_parity(USART1, USART_PARITY_NONE);
    usart_set_mode(USART1, USART_MODE_TX);
    usart_enable(USART1);
    uart_inited = 1;
}

void dbg_puts(const char *s) {
    if (!uart_inited) return;
    while (*s) {
        usart_send_blocking(USART1, (uint8_t)*s++);
    }
}

void dbg_hex(uint32_t v) {
    if (!uart_inited) return;
    char buf[12];
    int i = 10;
    buf[i--] = 0;
    buf[i--] = '\n';
    for (int d = 0; d < 8; d++) {
        int nib = (v >> (d*4)) & 0xf;
        buf[i--] = nib < 10 ? ('0'+nib) : ('a'+nib-10);
    }
    dbg_puts(&buf[i+1]);
}
