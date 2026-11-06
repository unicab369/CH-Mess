# `make` builds the images and flashes the functions plus the app.
all: flash

# Select the target chip and name the CodeFlash application.
TARGET_MCU := CH582
TARGET := ch582-dataflash-call

# Offsets are relative to the start of DataFlash (0x70000).
# The first function uses the known-working DataFlash base; the others use
# separate 256-byte pages after it.
DF_SUM_OFF ?= 0x000
DF_WEIGHT_OFF ?= 0x100
DF_BYTES_OFF ?= 0x200

# Supplies the CH582 compiler, linker script, standard build, and minichlink rules.
include ../../ch32fun/ch32fun/ch32fun.mk

# Replace ch32fun's default flash command: write each function at its matching
# DataFlash offset, write the app to CodeFlash, then reboot the chip.
FLASH_COMMAND := $(MINICHLINK)/minichlink -w $(TARGET)_sum.bin eeprom+$(DF_SUM_OFF) && \
	$(MINICHLINK)/minichlink -w $(TARGET)_weight.bin eeprom+$(DF_WEIGHT_OFF) && \
	$(MINICHLINK)/minichlink -w $(TARGET)_bytes.bin eeprom+$(DF_BYTES_OFF) && \
	$(MINICHLINK)/minichlink -w $(TARGET)_app.bin flash -b

# Make sure all four images exist before cv_flash runs FLASH_COMMAND.
cv_flash: $(TARGET)_app.bin $(TARGET)_sum.bin $(TARGET)_weight.bin $(TARGET)_bytes.bin

# Pass the same offsets to C so the app's function pointers match the flash locations.
EXTRA_CFLAGS += \
	-DDF_SUM_OFF=$(DF_SUM_OFF) \
	-DDF_WEIGHT_OFF=$(DF_WEIGHT_OFF) \
	-DDF_BYTES_OFF=$(DF_BYTES_OFF)

# Strip the ELF into a raw CodeFlash image; keep the separate storage section out.
$(TARGET)_app.bin: $(TARGET).elf
	$(PREFIX)-objcopy -R .storage -O binary $< $@

# Compile each standalone function source into an object file. Disable LTO so
# objcopy can extract ordinary machine code from the function's section.
df_%.o: df_%.c
	$(PREFIX)-gcc -c $(CFLAGS) -fno-lto -o $@ $<

# Extract the function and pad its image to a full 256-byte page. minichlink's
# CH5xx writer programs 4-byte words, so page padding prevents a short final
# word (for example, the last two bytes of the 6-byte sum function) being lost.
$(TARGET)_sum.bin: df_sum.o
	$(PREFIX)-objcopy -j .df_fn --gap-fill=0xff --pad-to=0x100 -O binary $< $@

$(TARGET)_weight.bin: df_weight.o
	$(PREFIX)-objcopy -j .df_fn --gap-fill=0xff --pad-to=0x100 -O binary $< $@

$(TARGET)_bytes.bin: df_bytes.o
	$(PREFIX)-objcopy -j .df_fn --gap-fill=0xff --pad-to=0x100 -O binary $< $@

# `flash` writes all images. `flash-dataflash` updates only the functions, so
# use it only when the app already contains the same offsets and signatures.
flash: cv_flash
flash-dataflash: $(TARGET)_sum.bin $(TARGET)_weight.bin $(TARGET)_bytes.bin $(MINICHLINK)/minichlink
	$(MINICHLINK)/minichlink -w $(TARGET)_sum.bin eeprom+$(DF_SUM_OFF)
	$(MINICHLINK)/minichlink -w $(TARGET)_weight.bin eeprom+$(DF_WEIGHT_OFF)
	$(MINICHLINK)/minichlink -w $(TARGET)_bytes.bin eeprom+$(DF_BYTES_OFF)

# Remove generated build outputs, including names left by earlier example versions.
clean: cv_clean
	$(RM) $(TARGET)_app.bin $(TARGET)_sum.bin $(TARGET)_weight.bin $(TARGET)_bytes.bin \
		$(TARGET)_weighted.bin $(TARGET)_array.bin $(TARGET)_dataflash.bin \
		$(TARGET)_v1.bin $(TARGET)_v2.bin \
		df_sum.o df_weight.o df_bytes.o dataflash_sum.o \
		dataflash_weighted.o dataflash_array.o dataflash_v1.o dataflash_v2.o
