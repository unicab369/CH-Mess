# `make` builds the app and combined DataFlash image, then flashes both.
all: flash

TARGET_MCU := CH582
TARGET := dataflash-call

# Offsets are relative to the DataFlash base address, 0x70000.
DATAFLASH_SUM_OFFSET ?= 0x000
DATAFLASH_CALLBACK_OFFSET ?= 0x040
DATAFLASH_BYTES_OFFSET ?= 0x080
DATAFLASH_CHECKSUM_OFFSET ?= 0x0c0
DATAFLASH_STRUCT_OFFSET ?= 0x100

include ../../ch32fun/ch32fun/ch32fun.mk

# The app and linker script use the same offsets for function addresses.
EXTRA_CFLAGS += -DDATAFLASH_SUM_OFFSET=$(DATAFLASH_SUM_OFFSET) \
	-DDATAFLASH_CALLBACK_OFFSET=$(DATAFLASH_CALLBACK_OFFSET) \
	-DDATAFLASH_BYTES_OFFSET=$(DATAFLASH_BYTES_OFFSET) \
	-DDATAFLASH_CHECKSUM_OFFSET=$(DATAFLASH_CHECKSUM_OFFSET) \
	-DDATAFLASH_STRUCT_OFFSET=$(DATAFLASH_STRUCT_OFFSET)

# Write the combined DataFlash image, then the CodeFlash app.
FLASH_COMMAND := $(MINICHLINK)/minichlink -w $(TARGET)_dataflash.bin eeprom && \
	$(MINICHLINK)/minichlink -w $(TARGET)_app.bin flash -b
cv_flash: $(TARGET)_app.bin $(TARGET)_dataflash.bin

# Convert the linked CodeFlash app to its raw binary image.
$(TARGET)_app.bin: $(TARGET).elf
	$(PREFIX)-objcopy -R .storage -O binary $< $@

# Compile every function body from the shared header into one object file.
dataflash_functions.o: dataflash_functions.h
	$(PREFIX)-gcc -x c -DDATAFLASH_FUNCTIONS_IMAGE -c $(CFLAGS) -fno-lto -o $@ $<

# Link each function section at its assigned DataFlash address.
$(TARGET)_dataflash.elf: dataflash_functions.o dataflash_functions.ld
	$(PREFIX)-gcc $(CFLAGS) -fno-lto -nostdlib -o $@ dataflash_functions.o \
		-Wl,-T,dataflash_functions.ld \
		-Wl,--defsym=DATAFLASH_SUM_ADDRESS=0x00070000+$(DATAFLASH_SUM_OFFSET) \
		-Wl,--defsym=DATAFLASH_CALLBACK_ADDRESS=0x00070000+$(DATAFLASH_CALLBACK_OFFSET) \
		-Wl,--defsym=DATAFLASH_BYTES_ADDRESS=0x00070000+$(DATAFLASH_BYTES_OFFSET) \
		-Wl,--defsym=DATAFLASH_CHECKSUM_ADDRESS=0x00070000+$(DATAFLASH_CHECKSUM_OFFSET) \
		-Wl,--defsym=DATAFLASH_STRUCT_ADDRESS=0x00070000+$(DATAFLASH_STRUCT_OFFSET) -lgcc

# Flatten the linked sections and fill the 64-byte slots with 0xff.
$(TARGET)_dataflash.bin: $(TARGET)_dataflash.elf
	$(PREFIX)-objcopy --gap-fill=0xff --pad-to=0x00070140 -O binary $< $@

flash: cv_flash
flash-dataflash: $(TARGET)_dataflash.bin $(MINICHLINK)/minichlink
	$(MINICHLINK)/minichlink -w $< eeprom

clean: cv_clean
	$(RM) $(TARGET)_app.bin $(TARGET)_dataflash.elf $(TARGET)_dataflash.bin \
		$(TARGET)_dataflash_sum.bin $(TARGET)_dataflash_callback.bin \
		$(TARGET)_dataflash_bytes.bin $(TARGET)_dataflash_checksum.bin \
		$(TARGET)_dataflash_struct.bin dataflash_functions.o \
		dataflash_sum.o dataflash_callback.o dataflash_bytes.o \
		dataflash_checksum.o dataflash_struct.o
