all : flash

TARGET:=iSLER-mess
TARGET_MCU:=CH582
TARGET_MCU_PACKAGE:=CH582F
ADDITIONAL_C_FILES += micro-ecc/uECC.c

include ../ch32fun/ch32fun/ch32fun.mk


flash : cv_flash
clean : cv_clean
