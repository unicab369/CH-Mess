all : flash

TARGET:=ch32-mess
TARGET_MCU:=CH32V003

ADDITIONAL_C_FILES+= 3_Nrf/nrf24l01_low_level.c 3_Nrf/nrf24l01.c
ADDITIONAL_C_FILES+= ../Mess-libs/i2c/lib_i2c.c

# USB stuff
ADDITIONAL_C_FILES+= ../Mess-libs/usb/rv003usb/rv003usb.S 
ADDITIONAL_C_FILES+= ../Mess-libs/usb/rv003usb/rv003usb.c
EXTRA_CFLAGS:=-I../Mess-libs/usb/lib -I../Mess-libs/usb/rv003usb

include ../ch32fun/ch32fun/ch32fun.mk

# optionbytes :
	#$(MINICHLINK)/minichlink -w +a55af708 option # Default (But what does it mean??)
# 	$(MINICHLINK)/minichlink -w +a55aff00 option # Enable bootloader, disable RESET
	#$(MINICHLINK)/minichlink -w +a55aef10 option # Enable bootloader, enable RESET
	#$(MINICHLINK)/minichlink -w +a55adf20 option # Disable bootloader, disable RESET
	#$(MINICHLINK)/minichlink -w +a55acf30 option # Disable bootloader, enable RESET

flash : cv_flash
clean : cv_clean
