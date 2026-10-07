# SuperArcade F412RGT6 Bootloader 修复说明（2026-10-06）

## 问题现象
按 A 从外部 SPI Flash 拷贝游戏到内部 Flash，进度 15% 卡死。串口日志：
```
fw: dst=08010000 sec=00000004 addr=08010000 sz=00004000 erased=00000000
non blank a=08010000 v=20040000
fw: erasing...
esr=00000000 ecr=00000020        <- 扇区4 擦除完成（SR无错误）
non blank a=08020000 v=42908d5a  <- 0x08020000（扇区8）擦前有旧数据
esr=00000000 ecr=00000040        <- 扇区8 擦除"完成"（BSY清、无错误）
non blank a=08020000 v=42908d5a  <- 但 0x08020000 内容仍旧值！
dbchk v@20000=42908d5a v@40000=f7fb4811  <- 0x08020000/0x08040000 全旧数据
erase fail
```

## 根因（两处，均已修）

### 1. 扇区表错误（本轮核心）
`main_f4.c` 中 `flash_sectors[]` 表原本硬编码 F405/F427 式布局
（扇区 4-7=16KB、8-9=64KB、10-15=128KB）。
**STM32F412RGT6（1MB）实际是 F401 式布局：**
- 扇区 0-3：16KB（0x08000000-0x08010000）
- 扇区 4：64KB（0x08010000-0x08020000）
- 扇区 5-11：128KB×7（0x08020000-0x08100000）

实测证据：擦 SN=4 能擦掉 0x08010000（说明扇区4实际64KB起），
而擦 SN=8 对 0x08020000 无效（F412 扇区8实际在 0x08080000）。
原工程 else 分支的表本来就是正确布局，错误的 BL_F412 分支已删除，
`boards/f412rgt6/board.h` 的 `BOARD_FLASH_SECTORS` 从 15 改回 11。

### 2. 擦除超时（已修，配套）
libopencm3 `flash_erase_sector` 用 `FLASH_TIMEOUT`（~10ms）等待 BSY，
16KB 扇区擦得完，但 64KB（~0.4s）/128KB（~0.8s）扇区必然超时
被清 SER 中止 → 擦后仍非 0xFF。已在 `main_f4.c` 自实现
`flash_erase_sector_bsy()`：无限等待 BSY 清除（附 ~10s 超时保护打印）。

## 改动文件
- `main_f4.c`：删 BL_F412 错表（统一用 F412 正确布局）；
  新增 `flash_erase_sector_bsy()`（flash_write 中使用）；
  `is_blank` 打印首个非 0xFF 的地址+值（原 DMESG 缓冲不可见）；
  时钟配置（BL_F412 分支）去掉 `FLASH_ACR_DCE`（D-Cache 会导致
  擦除后 is_blank 命中旧缓存值误判，双保险）；
  擦除后强制重使能 D-Cache 清缓存行。
- `boards/f412rgt6/board.h`：`BOARD_FLASH_SECTORS` 15 → 11。

## 编译
```
export PATH=<arm-none-eabi toolchain>/bin:$PATH
make BOARD=f412rgt6
# 产物：build/f412rgt6/bootloader.bin（32KB，0x08000000）
# 烧录：objcopy -O ihex 转 hex 后 ST-LINK 写入
```
注：`flasher.uf2` 目标链接失败是原工程问题（flasher 未包含
spi_flash.o），不影响 bootloader.bin 主产物。

## 预期修复后行为
A 拷贝写内部 Flash 时按正确扇区擦除：
0x08010000（扇区4，64KB）→ 0x08020000（扇区5，128KB）→ ... 
128KB 扇区擦除停顿 ~0.8s 后正常继续，不再 erase fail。
