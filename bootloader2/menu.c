/**
 * 多合一游戏菜单 (bootloader 内)
 */
#include <stdint.h>
#include <string.h>
#include "font_cn.h"
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/pwr.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/adc.h>
#define RTC_BKP0 MMIO32(RTC_BASE + 0x50)
#include <libopencm3/cm3/scb.h>

#include "bl.h"
#include "spi_flash.h"
#include "dbg_uart.h"

#define EXT_DIR_ADDR      0x020000
#define MAX_GAMES         32
#define DIR_ENTRY_SIZE   64

#define DIR_EMPTY    0
#define DIR_VALID    1

struct game_entry {
    char     name[32];
    uint32_t size;
    uint32_t offset;
    uint8_t  status;
};

extern uint8_t fb[];

static void menu_delay(unsigned ms) { screen_delay(ms); }

static uint8_t read_dir(int idx, struct game_entry *e) {
    uint8_t raw[DIR_ENTRY_SIZE];
    spi_flash_read(EXT_DIR_ADDR + idx * DIR_ENTRY_SIZE, raw, DIR_ENTRY_SIZE);
    memset(e, 0, sizeof(*e));
    memcpy(e->name, raw, 32);
    e->name[31] = 0;
    e->size   = ((uint32_t)raw[32]<<24)|((uint32_t)raw[33]<<16)|((uint32_t)raw[34]<<8)|raw[35];
    e->offset = ((uint32_t)raw[36]<<24)|((uint32_t)raw[37]<<16)|((uint32_t)raw[38]<<8)|raw[39];
    e->status = raw[40];
    return e->status;
}

static int list_games(struct game_entry *list, int max) {
    int n = 0;
    for (int i = 0; i < MAX_GAMES && n < max; i++) {
        struct game_entry e;
        if (read_dir(i, &e) == DIR_VALID && e.name[0])
            list[n++] = e;
    }
    return n;
}

static int btn_pressed(int cfg) { return pin_get(cfg) == 0; }

#define KEY_WIPE 0x100
#define KEY_DEL  0x101
#define SEL_PIN GPIOC, GPIO0
#define MENU_PIN GPIOC, GPIO1

static int wait_key(void) {
    while (btn_pressed(CFG_PIN_BTN_UP) || btn_pressed(CFG_PIN_BTN_DOWN) ||
           btn_pressed(CFG_PIN_BTN_A) || btn_pressed(CFG_PIN_BTN_B) ||
           gpio_get(SEL_PIN)==0 || gpio_get(MENU_PIN)==0)
        ;
    menu_delay(30);
    for (;;) {
        if (gpio_get(SEL_PIN) == 0) { while(gpio_get(SEL_PIN)==0); menu_delay(50); return KEY_WIPE; }
        if (gpio_get(MENU_PIN) == 0) { while(gpio_get(MENU_PIN)==0); menu_delay(50); return KEY_DEL; }
        if (btn_pressed(CFG_PIN_BTN_UP))    { while(btn_pressed(CFG_PIN_BTN_UP));    menu_delay(30); return CFG_PIN_BTN_UP; }
        if (btn_pressed(CFG_PIN_BTN_DOWN))  { while(btn_pressed(CFG_PIN_BTN_DOWN));  menu_delay(30); return CFG_PIN_BTN_DOWN; }
        if (btn_pressed(CFG_PIN_BTN_A))      { while(btn_pressed(CFG_PIN_BTN_A));      menu_delay(30); return CFG_PIN_BTN_A; }
        if (btn_pressed(CFG_PIN_BTN_B))      { while(btn_pressed(CFG_PIN_BTN_B));      menu_delay(30); return CFG_PIN_BTN_B; }
    }
}

extern void set_pixel_pub(int x, int y, int c);

static void print_cn(int x, int y, int col, const char *text) {
    while (*text) {
        uint32_t c = (uint8_t)*text++;
        if (c < 0x80) {
            char tmp[2] = {c, 0};
            print2(x, y, col, tmp);
            x += 12;
            continue;
        }
        uint32_t code = ((c & 0x0F) << 12) | (((uint8_t)*text & 0x3F) << 6);
        text++;
        code |= (*text & 0x3F);
        text++;
        const struct cn_char *ch = NULL;
        for (int i = 0; i < CN_FONT_COUNT; i++)
            if (cn_font[i].code == code) { ch = &cn_font[i]; break; }
        if (!ch) { x += 12; continue; }
        for (int row = 0; row < 12; row++) {
            uint16_t bits = (ch->data[row*2] << 8) | ch->data[row*2+1];
            for (int colx = 0; colx < 12; colx++)
                if (bits & (1 << (11 - colx)))
                    set_pixel_pub(x + colx, y + row, col);
        }
        x += 12;
    }
}

#define PAL_BG     8
#define PAL_TITLE  1
#define PAL_ITEM   7
#define PAL_SEL    5
#define PAL_BAR    4

/* ---- 电池检测 (ADC1 IN8 = PB00, 100K/100K 分压) ---- */
static int battery_ready = 0;
static uint32_t battery_mv(void) {
    if (!battery_ready) {
        rcc_periph_clock_enable(RCC_ADC1);
        rcc_periph_clock_enable(RCC_GPIOB);
        gpio_mode_setup(GPIOB, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO0);
        adc_power_off(ADC1);
        adc_set_clk_prescale(ADC_CCR_ADCPRE_BY6);
        adc_set_resolution(ADC1, ADC_CR1_RES_12BIT);
        adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_480CYC);
        adc_set_regular_sequence(ADC1, 1, (uint8_t[]){8});
        adc_power_on(ADC1);
        battery_ready = 1;
    }
    adc_start_conversion_regular(ADC1);
    while (!(ADC1_SR & ADC_SR_EOC)) ;
    uint32_t raw = adc_read_regular(ADC1);
    /* raw/4095 * 3300 * 2 (分压2:1) */
    return raw * 3300UL * 2UL / 4095UL;
}

static int battery_bars(void) {
    uint32_t mv = battery_mv();
    if (mv >= 4000) return 4;
    if (mv >= 3800) return 3;
    if (mv >= 3600) return 2;
    if (mv >= 3400) return 1;
    return 0;
}

/* 电池图标: 外框 26x12, 右侧正极头 */
static void draw_battery(int x, int y, int bars, int visible) {
    int bx = x, by = y;
    if (!visible) {
        /* 清掉该区域 */
        for (int j = 0; j < 14; j++)
            for (int i = 0; i < 28; i++)
                set_pixel_pub(bx + i, by + j, 0);
        return;
    }
    /* 外框 */
    for (int i = 0; i < 24; i++) { set_pixel_pub(bx+i, by, 7); set_pixel_pub(bx+i, by+11, 7); }
    for (int j = 0; j < 12; j++) { set_pixel_pub(bx, by+j, 7); set_pixel_pub(bx+23, by+j, 7); }
    /* 正极头 */
    for (int j = 3; j < 9; j++) { set_pixel_pub(bx+24, by+j, 7); set_pixel_pub(bx+25, by+j, 7); }
    /* 内部: 4格, 每格宽4, 间距1 */
    int cell = 4, gap = 1, off = 2;
    for (int k = 0; k < 4; k++) {
        if (k < bars) {
            for (int j = 2; j < 10; j++)
                for (int i = off; i < off + cell; i++)
                    set_pixel_pub(bx+i, by+j, 3);  /* 绿色 */
        }
        off += cell + gap;
    }
}

static void draw_menu(struct game_entry *games, int n, int sel, const char *status, int has_continue, int flash_ok) {
    memset(fb, 0, 320 * 120);
    drawBar(0, 34, PAL_BAR);
    print2(10, 8, 1, "Super Arcade Games");
    /* 右上角电池图标 */
    { static int flash_toggle = 0;
      int bars = battery_bars();
      int visible = (bars > 0) || (flash_toggle = !flash_toggle);
      draw_battery(284, 10, bars, visible);
    }

    int total = n + has_continue;
    if (!flash_ok) {
        print_cn(40, 100, 2, "外部Flash错误!");
    } else {
        if (n == 0 && has_continue) {
            /* show Continue + "No games" text */
            int y = 44;
            if (sel == 0) {
                for (int x = 0; x < 320; x++)
                    for (int j = 0; j < 13; j++)
                        set_pixel_pub(x, y + j, 5);
            }
            int col = (sel == 0) ? 0 : 7;
            print(8, y + 2, col, "> ");
            print_cn(20, y + 2, col, "继续上次游戏");
            print_cn(30, 90, 1, "外部存储无游戏");
            print_cn(30, 110, 7, "请通过USB下载游戏");
        } else if (total == 0) {
            print_cn(30, 90, 1, "外部存储无游戏");
            print_cn(30, 110, 7, "请通过USB下载游戏");
        } else {
        int start = 0;
        int max_vis = 9;
        if (sel >= max_vis) start = sel - max_vis + 1;
        for (int i = 0; i < max_vis && (start + i) < total; i++) {
            int row = start + i;
            int y = 44 + i * 16;
            int is_sel = (row == sel);
            if (is_sel) {
                for (int x = 0; x < 320; x++)
                    for (int j = 0; j < 13; j++)
                        set_pixel_pub(x, y + j, 5);
            }
            int col = is_sel ? 0 : 7;
            if (row < has_continue) {
                print(8, y + 2, col, "> ");
                print_cn(20, y + 2, col, "继续上次游戏");
            } else {
                int gi = row - has_continue;
                print(8, y + 2, col, games[gi].name);
                char sz[10]; int pos=0; int kb=games[gi].size/1024;
                if(kb>=100){sz[pos++]='0'+kb/100;kb%=100;}
                if(kb>=10){sz[pos++]='0'+kb/10;kb%=10;}
                sz[pos++]='0'+kb; sz[pos++]='K'; sz[pos]=0;
                print(280, y+2, col, sz);
            }
        }
        }
    }

    /* flash usage: Flash: [bar 102x10] XXXK/2048K */
    { uint32_t used=0;
      for(int i=0;i<n;i++) used+=games[i].size;
      int pct = used * 100 / (2048*1024);
      print2(10, 190, 7, "Flash:");
      int bx=84, by=189;
      for(int x=0; x<102; x++) { set_pixel_pub(bx+x, by, 7); set_pixel_pub(bx+x, by+9, 7); }
      for(int y=0; y<10; y++) { set_pixel_pub(bx, by+y, 7); set_pixel_pub(bx+101, by+y, 7); }
      int fill = pct; if(fill>100) fill=100;
      int fc = (pct>80)?2:3;
      for(int y=1; y<9; y++)
        for(int x=1; x<=fill; x++)
          set_pixel_pub(bx+x, by+y, fc);
      char buf[20]; int pos=0;
      int kb=used/1024;
      if(kb>=1000){ buf[pos++]='0'+kb/1000; kb%=1000; }
      if(kb>=100){ buf[pos++]='0'+kb/100; kb%=100; }
      if(kb>=10){ buf[pos++]='0'+kb/10; kb%=10; }
      buf[pos++]='0'+kb; buf[pos++]='K';
      buf[pos++]='/'; buf[pos++]='2'; buf[pos++]='0'; buf[pos++]='4'; buf[pos++]='8'; buf[pos++]='K';
      buf[pos]=0;
      print2(210, 190, 7, buf);
    }

    drawBar(206, 34, PAL_BAR);
    if (status) {
        print2(10, 214, 1, status);
    } else {
        print_cn(10, 214, 1, "运行:A 上传:B 删除:MENU 清空:SEL");
    }
    draw_screen();
}

extern void flash_write(uint32_t dst, const uint8_t *src, int len);

static int load_game_to_internal(const struct game_entry *g) {
    uint8_t buf[512];
    uint32_t off = 0;
    uint32_t size = g->size;
    if (size == 0 || size > 900 * 1024) return -1;
    extern int g_write_to_ext;
    g_write_to_ext = 0;
    while (off < size) {
        int chunk = 512;
        if (off + chunk > size) chunk = size - off;
        spi_flash_read(g->offset + off, buf, chunk);
        flash_write(APP_LOAD_ADDRESS + off, buf, chunk);
        off += chunk;
        if ((off & 0xfff) == 0) {
            char pct[24];
            int p = off * 100 / size;
            pct[0]='L'; pct[1]='o'; pct[2]='a'; pct[3]='d';
            pct[4]=' '; pct[5]='0'+(p/10); pct[6]='0'+(p%10); pct[7]='%'; pct[8]=0;
            drawBar(206, 34, PAL_BAR);
            print2(10, 214, 1, pct);
            draw_screen();
        }
    }
    return 0;
}

void game_menu(void) {
    dbg_uart_init();
    dbg_puts("\n=== game_menu ===\n");
    screen_init();
    int fl = spi_flash_detect();
    dbg_puts("spi_flash_detect="); dbg_hex(fl);

    setup_input_pin(CFG_PIN_BTN_UP);
    setup_input_pin(CFG_PIN_BTN_DOWN);
    setup_input_pin(CFG_PIN_BTN_A);
    setup_input_pin(CFG_PIN_BTN_B);
    gpio_mode_setup(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO0|GPIO1);

    static struct game_entry games[MAX_GAMES];

    PWR_CR |= PWR_CR_DBP;
    RCC_BDCR |= RCC_BDCR_RTCEN;
    uint32_t last_slot = RTC_BKP0;
    uint32_t upflag = MMIO32(RTC_BASE + 0x58);
    if (upflag == 0xF001) MMIO32(RTC_BASE + 0x58) = 0;   /* consume notice */
    RCC_BDCR &= ~RCC_BDCR_RTCEN;
    PWR_CR &= ~PWR_CR_DBP;
    const char *init_status = (upflag == 0xF001) ? "外部空间不足" : NULL;

    for (;;) {
        int n = fl ? list_games(games, MAX_GAMES) : 0;
        /* always show Continue option */
        int has_continue = 1;
        if (last_slot >= n) last_slot = 0;
        int total = n + has_continue;
        int sel = 0;
        draw_menu(games, n, sel, init_status, has_continue, fl);

        for (;;) {
            int k = wait_key();
            if (k == CFG_PIN_BTN_UP && total > 0) {
                sel = (sel + total - 1) % total;
                draw_menu(games, n, sel, NULL, has_continue, fl);
            } else if (k == CFG_PIN_BTN_DOWN && total > 0) {
                sel = (sel + 1) % total;
                draw_menu(games, n, sel, NULL, has_continue, fl);
            } else if (k == CFG_PIN_BTN_A && total > 0) {
                int idx = sel - has_continue;
                int is_continue = (sel == 0 && has_continue);
                if (is_continue) {
                    extern void jump_to_app(void);
                    jump_to_app();
                    draw_menu(games, n, sel, "Loading...", has_continue, fl);
                    if (n > 0 && load_game_to_internal(&games[last_slot]) == 0) jump_to_app();
                } else if (idx >= 0 && idx < n) {
                    PWR_CR |= PWR_CR_DBP; RTC_BKP0 = idx; PWR_CR &= ~PWR_CR_DBP;
                    draw_menu(games, n, sel, "Loading...", has_continue, fl);
                    if (load_game_to_internal(&games[idx]) == 0) {
                        extern void jump_to_app(void);
                        jump_to_app();
                    } else {
                        draw_menu(games, n, sel, "Bad size!", has_continue, fl);
                    }
                }
            } else if (k == CFG_PIN_BTN_B) {
                extern void ext_upload_begin(void);
                ext_upload_begin();
                extern int g_upload_failed;
                if (g_upload_failed) {
                    g_upload_failed = 0;
                    draw_menu(games, n, sel, "Flash full!", has_continue, fl);
                    menu_delay(2000);
                }
                return;
            } else if (k == KEY_DEL && sel >= has_continue) {
                int idx = sel - has_continue;
                int cancelled = 0;
                for(;;){
                    memset(fb, 0, 320*120);
                    drawBar(0, 34, PAL_BAR);
                    print_cn(80, 80, 1, "删除游戏?");
                    print(80, 110, 7, games[idx].name);
                    print_cn(80, 140, 7, "A=确认 B=取消");
                    draw_screen();
                    int ck = wait_key();
                    if (ck == CFG_PIN_BTN_A) {
                        uint8_t dir[32][64];
                        for(int s=0;s<32;s++) spi_flash_read(0x020000+s*64, dir[s], 64);
                        for(int s=0;s<32;s++){
                            if(dir[s][40]==1 && !memcmp(dir[s], games[idx].name, 32)){
                                spi_flash_erase_sector(0x020000);
                                for(int t=0;t<32;t++){
                                    if(t!=s && dir[t][40]==1) spi_flash_write(0x020000+t*64, dir[t], 64);
                                }
                                break;
                            }
                        }
                    } else {
                        cancelled = 1;
                    }
                    break;
                }
                /* refresh list and redraw */
                if (!cancelled) {
                    n = list_games(games, MAX_GAMES);
                    if (sel >= n + has_continue) sel = 0;
                }
                draw_menu(games, n, sel, NULL, has_continue, fl);
            } else if (k == KEY_WIPE) {
                int cancelled = 0;
                for(;;){
                    memset(fb, 0, 320*120);
                    drawBar(0, 34, PAL_BAR);
                    print_cn(80, 80, 1, "清空所有游戏?");
                    print_cn(80, 120, 7, "A=确认 B=取消");
                    draw_screen();
                    int ck = wait_key();
                    if (ck == CFG_PIN_BTN_A) {
                        drawBar(0, 34, PAL_BAR);
                        print2(80, 100, 1, "Wiping...");
                        draw_screen();
                        for(int s=0;s<32;s++) spi_flash_erase_sector(0x020000 + s*4096);
                        PWR_CR |= PWR_CR_DBP; RTC_BKP0 = 0xffffffff; PWR_CR &= ~PWR_CR_DBP;
                        extern void scb_reset_system(void);
                        scb_reset_system();
                    } else {
                        cancelled = 1;
                    }
                    break;
                }
                if (!cancelled) {
                    n = 0;
                }
                draw_menu(games, n, 0, NULL, has_continue, fl);
            }
        }
    }
}
