all: flash

TARGET_MCU := CH582
TARGET := ch582-dataflash-call
DATAFLASH_VARIANT ?= v1

ifeq ($(DATAFLASH_VARIANT),v1)
DATAFLASH_SOURCE := dataflash_v1.c
else ifeq ($(DATAFLASH_VARIANT),v2)
DATAFLASH_SOURCE := dataflash_v2.c
else
$(error DATAFLASH_VARIANT must be v1 or v2)
endif
DATAFLASH_OBJECT := $(DATAFLASH_SOURCE:.c=.o)

include ../../ch32fun/ch32fun/ch32fun.mk

# Program the selected function image to DataFlash, then the app to CodeFlash.
FLASH_COMMAND := $(MINICHLINK)/minichlink -w $(TARGET)_$(DATAFLASH_VARIANT).bin eeprom && \
	$(MINICHLINK)/minichlink -w $(TARGET)_app.bin flash -b
cv_flash: $(TARGET)_app.bin $(TARGET)_$(DATAFLASH_VARIANT).bin

$(TARGET)_app.bin: $(TARGET).elf
	$(PREFIX)-objcopy -R .storage -O binary $< $@

dataflash_%.o: dataflash_%.c
	$(PREFIX)-gcc -c $(CFLAGS) -fno-lto -o $@ $<

$(TARGET)_$(DATAFLASH_VARIANT).bin: $(DATAFLASH_OBJECT)
	$(PREFIX)-objcopy -j .dataflash_fn -O binary $< $@

flash: cv_flash
flash-dataflash: $(TARGET)_$(DATAFLASH_VARIANT).bin $(MINICHLINK)/minichlink
	$(MINICHLINK)/minichlink -w $< eeprom

clean: cv_clean
	$(RM) $(TARGET)_app.bin $(TARGET)_v1.bin $(TARGET)_v2.bin dataflash_v1.o dataflash_v2.o
