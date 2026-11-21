TARGET_MCU := CH32V003
TARGET := sd-card-test
EXTRA_ELF_DEPENDENCIES := \
	../../../Mess-libs/spi/spi_port.h \
	../../../Mess-libs/spi/sd_card/mmcbbp.h \
	../../../Mess-libs/spi/sd_card/pff.h

include ../../../ch32fun/ch32fun/ch32fun.mk

flash: cv_flash
clean: cv_clean
