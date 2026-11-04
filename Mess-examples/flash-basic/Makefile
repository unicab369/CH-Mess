all : flash

TARGET_MCU:=CH582
TARGET:=flash-basic

include ../../ch32fun/ch32fun/ch32fun.mk

LINKER_SCRIPT := flash-basic-reserve.ld
$(TARGET).elf: $(GENERATED_LD_FILE)

flash : cv_flash
clean : cv_clean
