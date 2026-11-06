# `make` builds the images and flashes the functions plus the app.
all: flash

# Select the target chip and name the CodeFlash application.
TARGET_MCU := CH582
TARGET := ch582-dataflash-call

# Offsets are relative to the start of DataFlash (0x70000).
# The first function uses the DataFlash base; each function has its own page.
DATAFLASH_SUM_OFFSET ?= 0x000
DATAFLASH_BYTES_OFFSET ?= 0x200
DATAFLASH_CHECKSUM_OFFSET ?= 0x300
DATAFLASH_STRUCT_OFFSET ?= 0x400

# Supplies the CH582 compiler, linker script, standard build, and minichlink rules.
include ../../ch32fun/ch32fun/ch32fun.mk

# Replace ch32fun's default flash command: write each function at its matching
# DataFlash offset, write the app to CodeFlash, then reboot the chip.
FLASH_COMMAND := $(MINICHLINK)/minichlink -w $(TARGET)_dataflash_sum.bin eeprom+$(DATAFLASH_SUM_OFFSET) && \
	$(MINICHLINK)/minichlink -w $(TARGET)_dataflash_bytes.bin eeprom+$(DATAFLASH_BYTES_OFFSET) && \
	$(MINICHLINK)/minichlink -w $(TARGET)_dataflash_checksum.bin eeprom+$(DATAFLASH_CHECKSUM_OFFSET) && \
	$(MINICHLINK)/minichlink -w $(TARGET)_dataflash_struct.bin eeprom+$(DATAFLASH_STRUCT_OFFSET) && \
	$(MINICHLINK)/minichlink -w $(TARGET)_app.bin flash -b

# Make sure the app and all four function images exist before flashing.
cv_flash: $(TARGET)_app.bin $(TARGET)_dataflash_sum.bin \
	$(TARGET)_dataflash_bytes.bin \
	$(TARGET)_dataflash_checksum.bin $(TARGET)_dataflash_struct.bin

# Pass the same offsets to C so the app's function pointers match the flash locations.
EXTRA_CFLAGS += \
	-DDATAFLASH_SUM_OFFSET=$(DATAFLASH_SUM_OFFSET) \
	-DDATAFLASH_BYTES_OFFSET=$(DATAFLASH_BYTES_OFFSET) \
	-DDATAFLASH_CHECKSUM_OFFSET=$(DATAFLASH_CHECKSUM_OFFSET) \
	-DDATAFLASH_STRUCT_OFFSET=$(DATAFLASH_STRUCT_OFFSET)

# Strip the ELF into a raw CodeFlash image; keep the separate storage section out.
$(TARGET)_app.bin: $(TARGET).elf
	$(PREFIX)-objcopy -R .storage -O binary $< $@

# Compile each standalone function source into an object file. Disable LTO so
# objcopy can extract ordinary machine code from the function's section.
dataflash_%.o: dataflash_%.c
	$(PREFIX)-gcc -c $(CFLAGS) -fno-lto -o $@ $<

# Compile the shared header as C for the standalone struct-update image.
dataflash_struct.o: dataflash_struct.h
	$(PREFIX)-gcc -x c -DDATAFLASH_STRUCT_IMAGE -c $(CFLAGS) -fno-lto -o $@ $<

# Extract the function and pad its image to a full 256-byte page. minichlink's
# CH5xx writer programs 4-byte words, so page padding prevents a short final
# word (for example, the last two bytes of the 6-byte sum function) being lost.
$(TARGET)_dataflash_sum.bin: dataflash_sum.o
	$(PREFIX)-objcopy -j .dataflash_fn --gap-fill=0xff --pad-to=0x100 -O binary $< $@

$(TARGET)_dataflash_bytes.bin: dataflash_bytes.o
	$(PREFIX)-objcopy -j .dataflash_fn --gap-fill=0xff --pad-to=0x100 -O binary $< $@

$(TARGET)_dataflash_checksum.bin: dataflash_checksum.o
	$(PREFIX)-objcopy -j .dataflash_fn --gap-fill=0xff --pad-to=0x100 -O binary $< $@

$(TARGET)_dataflash_struct.bin: dataflash_struct.o
	$(PREFIX)-objcopy -j .dataflash_fn --gap-fill=0xff --pad-to=0x100 -O binary $< $@

# `flash` writes all images. `flash-dataflash` updates only the functions, so
# use it only when the app already contains the same offsets and signatures.
flash: cv_flash
flash-dataflash: $(TARGET)_dataflash_sum.bin $(TARGET)_dataflash_bytes.bin \
	$(TARGET)_dataflash_checksum.bin \
	$(TARGET)_dataflash_struct.bin $(MINICHLINK)/minichlink
	$(MINICHLINK)/minichlink -w $(TARGET)_dataflash_sum.bin eeprom+$(DATAFLASH_SUM_OFFSET)
	$(MINICHLINK)/minichlink -w $(TARGET)_dataflash_bytes.bin eeprom+$(DATAFLASH_BYTES_OFFSET)
	$(MINICHLINK)/minichlink -w $(TARGET)_dataflash_checksum.bin eeprom+$(DATAFLASH_CHECKSUM_OFFSET)
	$(MINICHLINK)/minichlink -w $(TARGET)_dataflash_struct.bin eeprom+$(DATAFLASH_STRUCT_OFFSET)

# Remove generated build outputs, including names left by earlier example versions.
clean: cv_clean
	$(RM) $(TARGET)_app.bin $(TARGET)_dataflash_sum.bin \
		$(TARGET)_dataflash_bytes.bin \
		$(TARGET)_dataflash_checksum.bin $(TARGET)_dataflash_struct.bin \
		dataflash_sum.o dataflash_bytes.o \
		dataflash_checksum.o dataflash_struct.o
