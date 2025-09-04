all : flash

TARGET:=example
TARGET_MCU:=CH32V003

ADDITIONAL_C_FILES+=../../Mess-libs/usb/rv003usb/rv003usb.S 
ADDITIONAL_C_FILES+=../../Mess-libs/usb/rv003usb/rv003usb.c
EXTRA_CFLAGS:=-I../../Mess-libs/usb/lib -I../../Mess-libs/usb/rv003usb

include ../../ch32fun/ch32fun/ch32fun.mk

flash : cv_flash
clean : cv_clean


