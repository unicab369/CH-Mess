all : flash

TARGET_MCU:=CH32V003
TARGET:=ch32-mess

ADDITIONAL_C_FILES+= 1_Foundation/modPWM.h 1_Foundation/modEncoder.h
ADDITIONAL_C_FILES+= 2_Device/fun_ws2812.h 
ADDITIONAL_C_FILES+= 3_Nrf/nrf24l01_low_level.c 3_Nrf/nrf24l01.c
ADDITIONAL_C_FILES+= ../Mess-libs/i2c/lib_i2c.c

include ../ch32fun/ch32fun/ch32fun.mk

# optionbytes :
	#$(MINICHLINK)/minichlink -w +a55af708 option # Default (But what does it mean??)
# 	$(MINICHLINK)/minichlink -w +a55aff00 option # Enable bootloader, disable RESET
	#$(MINICHLINK)/minichlink -w +a55aef10 option # Enable bootloader, enable RESET
	#$(MINICHLINK)/minichlink -w +a55adf20 option # Disable bootloader, disable RESET
	#$(MINICHLINK)/minichlink -w +a55acf30 option # Disable bootloader, enable RESET

flash : cv_flash
clean : cv_clean
