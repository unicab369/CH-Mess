all: $(TARGET).elf

TARGET_MCU := CH582
TARGET := flash-tag-benchmark

include ../../ch32fun/ch32fun/ch32fun.mk

$(TARGET).elf: tag-benchmark-data.h

# Build the benchmark firmware without automatically programming the chip.
flash: cv_flash

clean: cv_clean
