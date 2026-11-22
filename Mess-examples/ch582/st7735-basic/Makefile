TARGET_MCU := CH582
TARGET := st7735-basic
EXTRA_ELF_DEPENDENCIES := \
	../../../Mess-libs/spi/spi_port.h \
	../../../Mess-libs/spi/fun_st77352.h \
	../../../Mess-libs/spi/lib/lib_tft.h

include ../../../ch32fun/ch32fun/ch32fun.mk

flash: cv_flash
clean: cv_clean
