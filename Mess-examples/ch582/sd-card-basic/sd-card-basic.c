#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ch32fun.h"

#define SD_CARD_CS_PIN PB7
#define SPI_CS_HIGH() funDigitalWrite(SD_CARD_CS_PIN, FUN_HIGH)
#define SPI_CS_LOW()  funDigitalWrite(SD_CARD_CS_PIN, FUN_LOW)

#include "../../../Mess-libs/spi/spi_port.h"
#include "../../../Mess-libs/spi/sd_card/mmcbbp.h"

#define SD_TEST_FILE "HELLO.TXT"

static FATFS filesystem;

static uint8_t probe_idle_command(void) {
	static const uint8_t command[] = { 0x40, 0, 0, 0, 0, 0x95 }; // CMD0
	uint8_t response = 0xFF;

	SPI_SET_PRESCALER(7);
	SPI_CS_HIGH();
	for (unsigned i = 0; i < 10; ++i) (void)SPI_TRANSFER_8(0xFF);
	SPI_CS_LOW();
	(void)SPI_TRANSFER_8(0xFF);
	for (unsigned i = 0; i < sizeof(command); ++i)
		(void)SPI_TRANSFER_8(command[i]);
	for (unsigned i = 0; i < 10 && (response & 0x80); ++i)
		response = SPI_RECEIVE_8();
	SPI_CS_HIGH();
	(void)SPI_TRANSFER_8(0xFF);
	SPI_SET_PRESCALER(0);
	return response;
}

int main(void) {
	uint8_t sector[512];
	static const char message[] = "Hello from CH582 SD card test!\r\n";
	char readback[sizeof(message)];
	DSTATUS status;
	DRESULT read_result;
	FRESULT fs_result;
	UINT bytes_written = 0;
	UINT bytes_read = 0;

	SystemInit();
	funPinMode(SD_CARD_CS_PIN, GPIO_CFGLR_OUT_2Mhz_PP);
	SPI_CS_HIGH();
	SPI_INIT();
	SPI_SET_FRAME_SIZE(SPI_FRAME_8BIT);
	Delay_Ms(10);
	uint8_t cs_high = (uint8_t)funDigitalRead(SD_CARD_CS_PIN);
	SPI_CS_LOW();
	uint8_t cs_low = (uint8_t)funDigitalRead(SD_CARD_CS_PIN);
	SPI_CS_HIGH();
	printf("CS pin readback: %u -> %u\n", cs_high, cs_low);

	printf("Initializing SD card...\n");
	uint8_t cmd0_response = probe_idle_command();
	printf("CMD0 response: 0x%02X\n", cmd0_response);
	status = disk_initialize();
	if (status & STA_NOINIT) {
		printf("SD card initialization failed (status 0x%02X)\n", status);
		while (1) { }
	}
	printf("SD card initialized\n");

	read_result = disk_readp(sector, 0, 0, sizeof(sector));
	if (read_result != RES_OK) {
		printf("Reading sector 0 failed (result %u)\n", (unsigned)read_result);
		while (1) { }
	}
	printf("Sector 0 read OK; signature %02X%02X\n", sector[511], sector[510]);

	printf("Mounting existing FAT filesystem...\n");
	fs_result = pf_mount(&filesystem);
	if (fs_result != FR_OK) {
		printf("Mount failed (Petit FatFs result %u)\n", (unsigned)fs_result);
		while (1) { }
	}

	printf("Creating %s (%u bytes)...\n", SD_TEST_FILE,
	       (unsigned)(sizeof(message) - 1));
	fs_result = pf_create(SD_TEST_FILE, sizeof(message) - 1);
	if (fs_result == FR_EXIST) {
		/* Reuse the file from an earlier run. */
		fs_result = pf_open(SD_TEST_FILE);
	} else if (fs_result == FR_OK) {
		printf("Created %s\n", SD_TEST_FILE);
	}
	if (fs_result != FR_OK) {
		printf("Create/open error %u\n", (unsigned)fs_result);
		while (1) { }
	}

	fs_result = pf_write(message, sizeof(message) - 1, &bytes_written);
	if (fs_result != FR_OK || bytes_written != sizeof(message) - 1) {
		printf("Write failed (result %u, wrote %u of %u bytes)\n",
		       (unsigned)fs_result, (unsigned)bytes_written,
		       (unsigned)(sizeof(message) - 1));
		while (1) { }
	}
	fs_result = pf_write(0, 0, &bytes_written);
	if (fs_result != FR_OK) {
		printf("Write finalize failed (result %u)\n", (unsigned)fs_result);
		while (1) { }
	}

	printf("Reading %s back...\n", SD_TEST_FILE);
	fs_result = pf_open(SD_TEST_FILE);
	if (fs_result != FR_OK) {
		printf("Open for read failed (result %u)\n", (unsigned)fs_result);
		while (1) { }
	}
	fs_result = pf_read(readback, sizeof(message) - 1, &bytes_read);
	if (fs_result != FR_OK || bytes_read != sizeof(message) - 1) {
		printf("Read failed (result %u, read %u of %u bytes)\n",
		       (unsigned)fs_result, (unsigned)bytes_read,
		       (unsigned)(sizeof(message) - 1));
		while (1) { }
	}
	readback[bytes_read] = '\0';
	printf("Read back: %s", readback);
	if (strcmp(readback, message) != 0) {
		printf("Readback does not match the written string\n");
		while (1) { }
	}
	printf("SD card string write/read test passed\n");

	while (1) { }
}
