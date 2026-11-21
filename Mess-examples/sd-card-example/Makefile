TARGET_MCU := CH582
TARGET := sd-card-example

include ../../ch32fun/ch32fun/ch32fun.mk

CFLAGS += -I../../Mess-libs/sd_card

$(TARGET).elf: sd-card-example.c \
	../../Mess-libs/sd_card/mmc/mmcbbp.h \
	../../Mess-libs/sd_card/mmc/pff.h \
	../../Mess-libs/sd_card/mmc/diskio.h \
	../../Mess-libs/sd_card/mmc/sd_spi_port.h

all: $(TARGET).elf

flash: cv_flash
clean: cv_clean
