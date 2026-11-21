TARGET_MCU := CH582
TARGET := sd-card-example

include ../../ch32fun/ch32fun/ch32fun.mk

CFLAGS += -I../../Mess-libs/sd_card

all: $(TARGET).elf

flash: cv_flash
clean: cv_clean
