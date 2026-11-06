all: flash

TARGET_MCU := CH582
TARGET := ch582-dataflash-call
DF_SUM_OFF ?= 0x400
DF_WEIGHT_OFF ?= 0x500
DF_BYTES_OFF ?= 0x600

include ../../ch32fun/ch32fun/ch32fun.mk

# Program all three function images, then the app.
FLASH_COMMAND := $(MINICHLINK)/minichlink -w $(TARGET)_sum.bin eeprom+$(DF_SUM_OFF) && \
	$(MINICHLINK)/minichlink -w $(TARGET)_weight.bin eeprom+$(DF_WEIGHT_OFF) && \
	$(MINICHLINK)/minichlink -w $(TARGET)_bytes.bin eeprom+$(DF_BYTES_OFF) && \
	$(MINICHLINK)/minichlink -w $(TARGET)_app.bin flash -b
cv_flash: $(TARGET)_app.bin $(TARGET)_sum.bin $(TARGET)_weight.bin $(TARGET)_bytes.bin

EXTRA_CFLAGS += \
	-DDF_SUM_OFF=$(DF_SUM_OFF) \
	-DDF_WEIGHT_OFF=$(DF_WEIGHT_OFF) \
	-DDF_BYTES_OFF=$(DF_BYTES_OFF)

$(TARGET)_app.bin: $(TARGET).elf
	$(PREFIX)-objcopy -R .storage -O binary $< $@

df_%.o: df_%.c
	$(PREFIX)-gcc -c $(CFLAGS) -fno-lto -o $@ $<

$(TARGET)_sum.bin: df_sum.o
	$(PREFIX)-objcopy -j .df_fn -O binary $< $@

$(TARGET)_weight.bin: df_weight.o
	$(PREFIX)-objcopy -j .df_fn -O binary $< $@

$(TARGET)_bytes.bin: df_bytes.o
	$(PREFIX)-objcopy -j .df_fn -O binary $< $@

flash: cv_flash
flash-dataflash: $(TARGET)_sum.bin $(TARGET)_weight.bin $(TARGET)_bytes.bin $(MINICHLINK)/minichlink
	$(MINICHLINK)/minichlink -w $(TARGET)_sum.bin eeprom+$(DF_SUM_OFF)
	$(MINICHLINK)/minichlink -w $(TARGET)_weight.bin eeprom+$(DF_WEIGHT_OFF)
	$(MINICHLINK)/minichlink -w $(TARGET)_bytes.bin eeprom+$(DF_BYTES_OFF)

clean: cv_clean
	$(RM) $(TARGET)_app.bin $(TARGET)_sum.bin $(TARGET)_weight.bin $(TARGET)_bytes.bin \
		$(TARGET)_weighted.bin $(TARGET)_array.bin $(TARGET)_dataflash.bin \
		$(TARGET)_v1.bin $(TARGET)_v2.bin \
		df_sum.o df_weight.o df_bytes.o dataflash_sum.o \
		dataflash_weighted.o dataflash_array.o dataflash_v1.o dataflash_v2.o
