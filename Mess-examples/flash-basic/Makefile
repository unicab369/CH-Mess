all : flash

TARGET_MCU:=CH582
TARGET:=flash-basic

# Total CodeFlash reserved for the log, in erase blocks.
FLASH_LOG_SIZE := 8192
FLASH_BLOCK_SIZE := 4096

include ../../ch32fun/ch32fun/ch32fun.mk

# Use ch32fun's selected memory map, then append the log reservation assertion.
LINKER_SCRIPT := flash-basic-reserve.generated.ld
$(LINKER_SCRIPT): $(GENERATED_LD_FILE) flash-basic-reserve.ld
	cat $^ > $@

CFLAGS += -DFLASH_LOG_SIZE=$(FLASH_LOG_SIZE) -DFLASH_BLOCK_SIZE=$(FLASH_BLOCK_SIZE)
LDFLAGS += -Wl,--defsym=__flash_log_size=$(FLASH_LOG_SIZE),--defsym=__flash_block_size=$(FLASH_BLOCK_SIZE)

# Recreate the combined script when ch32fun regenerates its chip memory map.
$(TARGET).elf: $(LINKER_SCRIPT)

flash : cv_flash
clean : cv_clean
