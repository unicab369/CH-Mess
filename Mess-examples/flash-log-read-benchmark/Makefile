TARGET_MCU := CH582
TARGET := flash-log-read-benchmark

all: $(TARGET).elf

# Reserve three 4 KB blocks for the two log classes and their shared GC spare.
FLASH_BLOCK_SIZE := 4096
FLASH_LOG_SIZE := 12288

include ../../ch32fun/ch32fun/ch32fun.mk

LINKER_SCRIPT := flash-log-read-benchmark.generated.ld
$(LINKER_SCRIPT): $(GENERATED_LD_FILE) flash-log-read-benchmark.ld
	cat $^ > $@

CFLAGS += -DFLASH_LOG_SIZE=$(FLASH_LOG_SIZE) \
	-DFLASH_BLOCK_SIZE=$(FLASH_BLOCK_SIZE) \
	-DFLASH_LOG_BENCH_READ_COUNTERS
LDFLAGS += -Wl,--defsym=__flash_log_size=$(FLASH_LOG_SIZE),--defsym=__flash_block_size=$(FLASH_BLOCK_SIZE)

$(TARGET).elf: $(LINKER_SCRIPT)

# Build without automatically programming the chip.
flash: cv_flash

clean: cv_clean
	$(RM) $(LINKER_SCRIPT)
