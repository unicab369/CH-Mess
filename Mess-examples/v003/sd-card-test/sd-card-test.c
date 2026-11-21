#include <stdint.h>
#include <stdio.h>

#include "ch32fun.h"

#define SD_CARD_CS_PIN PD3
#define SPI_CS_HIGH() funDigitalWrite(SD_CARD_CS_PIN, FUN_HIGH)
#define SPI_CS_LOW()  funDigitalWrite(SD_CARD_CS_PIN, FUN_LOW)

#include "../../../Mess-libs/spi/spi_port.h"
#include "../../../Mess-libs/spi/sd_card/mmc/mmcbbp.h"

static FATFS filesystem;

int main(void) {
	uint8_t sector[512];
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
	printf("SD card test passed\n");

	while (1) { }
}
