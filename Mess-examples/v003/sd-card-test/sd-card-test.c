#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ch32fun.h"

#define SD_CARD_CS_PIN PD3
#define SPI_CS_HIGH() funDigitalWrite(SD_CARD_CS_PIN, FUN_HIGH)
#define SPI_CS_LOW()  funDigitalWrite(SD_CARD_CS_PIN, FUN_LOW)

#include "../../../Mess-libs/spi/spi_port.h"
#include "../../../Mess-libs/spi/sd_card/mmcbbp.h"

#define SD_TEST_FILE "HELLO.TXT"

static FATFS filesystem;

int main(void) {
	uint8_t sector[512];
	static const char message[] = "Hello from CH32V003 SD card test!\r\n";
	char readback[sizeof(message)];
	UINT bytes_written = 0;
	UINT bytes_read = 0;
	DSTATUS status;
	DRESULT read_result;
	FRESULT fs_result;

	SystemInit();
	funGpioInitAll();
	funPinMode(SD_CARD_CS_PIN, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
	SPI_CS_HIGH();

	printf("Initializing SD card...\n");
	SPI_INIT();
	SPI_SET_FRAME_SIZE(SPI_FRAME_8BIT);
	Delay_Ms(10);
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
		/* Reuse the file from a previous run; it must have enough capacity. */
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
	fs_result = pf_write(0, 0, &bytes_written); /* Finalize the sector write. */
	if (fs_result != FR_OK) {
		printf("Write finalize failed (Petit FatFs result %u)\n", (unsigned)fs_result);
		while (1) { }
	}

	printf("Reading %s back...\n", SD_TEST_FILE);
	fs_result = pf_open(SD_TEST_FILE); /* Reopening resets the file pointer to zero. */
	if (fs_result != FR_OK) {
		printf("Open for read failed (Petit FatFs result %u)\n", (unsigned)fs_result);
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
