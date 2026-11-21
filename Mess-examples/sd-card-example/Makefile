TARGET_MCU := CH582
TARGET := sd-card-example

include ../../ch32fun/ch32fun/ch32fun.mk

CFLAGS += -I../../Mess-libs/spi/sd_card

$(TARGET).elf: sd-card-example.c \
	../../Mess-libs/spi/sd_card/mmcbbp.h \
	../../Mess-libs/spi/sd_card/pff.h \
	../../Mess-libs/spi/spi_port.h

all: $(TARGET).elf

flash: cv_flash
clean: cv_clean
