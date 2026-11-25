all : flash

TARGET_MCU:=CH32V003
TARGET:=example

ADDITIONAL_C_FILES+= ../lib_i2c.c

include ../../../ch32fun/ch32fun/ch32fun.mk

flash : cv_flash
clean : cv_clean
