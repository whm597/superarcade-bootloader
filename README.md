# superarcade-bootloader — F412 多合一菜单 Bootloader（源码 + 固件）

> 本仓库源码 = 用户原始版（2026-10-07 回退确认与原始版**逐字节一致**）。当前唯一差异是仓库多带编译产物与说明文档，功能代码无任何改动。

## 项目背景

SuperArcade 掌机（STM32F412RGT6）上电先跑这个 bootloader（0x08000000 起始），提供多合一游戏菜单与烧录管理：

- **外部 SPI Flash（W25Q16）布局**：目录区 `0x020000`（32 个 slot × 64B，记录每个游戏槽状态），游戏数据区 `0x030000` 起
- **内部 FLASH 布局**：bootloader 占 0x08000000 区段，App 起始 `APP_LOAD_ADDRESS = 0x08010000`（`jump_to_app()` 跳转）
- **按键功能**：
  - 按 **B**：进入 USB 拖拽模式，可把 UF2 写到外部 Flash（游戏槽）
  - 按 **A**：把外部 Flash 当前选中游戏拷贝到内部 Flash 并直接运行
  - 按 **SEL**：清空游戏目录（带确认）
  - **Continue**：直接跳转内部 Flash 运行已烧录游戏
- **启动逻辑**：不插 USB → 进菜单；插 USB → 进 bootloader（可拖 UF2）
- **显示**：320×240 TFT（ILI9341，FSMC 4bpp，真 320×240）

## 文件清单

| 文件/目录 | 说明 |
|---|---|
| `bootloader2/` | **完整源码**。核心文件：`main_f4.c`（菜单/USB/擦写逻辑）、`menu.c`、`spi_flash.c`（外部 Flash 驱动）、`bl.c`、`ghostfat.c`（UF2 拖拽）、`hf2.c`、`usb.c`、`boards/f412rgt6/`（板级配置） |
| `bootloader2/main_f4.c.modified-buggy.bak` | **出过问题版本的备份**（0x800701B1 的根因版本，仅供对比分析，不要编译使用） |
| `bootloader_f412rgt6_restored.hex` | 编译好的固件（MD5 `0f67d8789b5e7123f3fcba7485de42c5`），刷 0x08000000 |
| `bootloader.bin` | 同内容 bin（MD5 `294cffe6abf39ab4d61506c11990fd26`） |

## 硬件要点（踩坑记录）

- 芯片 STM32F412RGT6；bootloader 编译目标 `BOARD=f412rgt6`（**不要用默认 f401**）
- 扇区表：{0x01,16K}{0x02,16K}{0x03,16K}{0x04,64K}{0x05-0x0B,128K} 共 11 个扇区 = 1MB 全可擦
- 外部 Flash 软 SPI：SCK=PA15 / MOSI=PB3 / MISO=PB5 / CS=PB4
- 系统时钟：TIM5 PSC=57（board.mk 保持默认 F401 的 84MHz 配置）

## 编译方法

```bash
cd bootloader2
export PATH=/path/to/arm-none-eabi/bin:$PATH   # 例：arm-gnu-toolchain-11.3
make BOARD=f412rgt6
```

产物：`build/f412rgt6/bootloader.bin`、`bootloader.elf`。hex 用 objcopy 生成：

```bash
arm-none-eabi-objcopy -O ihex build/f412rgt6/bootloader.elf bootloader.hex
```

> 注意：`flasher.uf2` 附属目标因 flasher.c 链接列表缺 `spi_flash_*` 实现会报错，**不影响主 bootloader 固件**。

## 刷写方法

- **ST-Link**：`st-flash write bootloader_f412rgt6_restored.hex`（或用 OpenOCD：`program bootloader.elf verify reset exit`）
- **DFU**：进入系统 bootloader 后烧 hex 到 0x08000000
- 刷完验证：上电应出现多合一菜单（或插 USB 进拖拽模式）

## 踩坑记录（0x800701B1 问题）

**现象**：拖 UF2 到内部 Flash 报 Windows 错误 0x800701B1（指定不存在的设备）；下载到外部 Flash 后按 A 加载卡 99%。

**根因**：某次修改在擦除路径新增了 `flash_erase_sector_bsy` 自实现 + DCE 缓存关-开 + `PANIC("erase")` 死循环。PANIC 宏 = `for(;;)` 无限死循环——擦除后 `is_blank` 若命中 ART 数据缓存旧值误判 → PANIC → USB 停止响应 → Windows 判定设备不存在。libopencm3 原版 `flash_wait_for_last_operation` 是**无限等待 BSY（无超时）**，擦除本身可靠，自实现属多余。

**修复**：`main_f4.c` 回退到原始版（本仓库即修复版），问题版保留在 `main_f4.c.modified-buggy.bak` 供分析。

## 与云端/编辑器关系（防分叉提醒）

- bootloader 与 target.js 是**两条独立代码线**：改 bootloader 不动 target.js，反之亦然
- 刷内部 Flash 失败的绕行建议：ST-Link 直烧 / 云编译固件拖盘
