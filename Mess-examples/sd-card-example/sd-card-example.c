#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ch32fun.h"

#define SD_CARD_CS_PIN PA5
#define SPI_CS_HIGH() funDigitalWrite(SD_CARD_CS_PIN, FUN_HIGH)
#define SPI_CS_LOW()  funDigitalWrite(SD_CARD_CS_PIN, FUN_LOW)

#include "../../Mess-libs/spi/spi_port.h"
#include "../../Mess-libs/spi/sd_card/mmc/mmcbbp.h"

#define DEMO_FILE "HELLO.TXT"
#define FORMAT_PARTITION_START 2048UL
#define FORMAT_PARTITION_SECTORS 524288UL /* 256 MiB FAT16 partition */
#define FAT16_SECTORS_PER_CLUSTER 8u
#define FAT16_SECTORS_PER_FAT 256u
#define FAT16_ROOT_DIR_SECTORS 32u
static FATFS filesystem;

static void put_u16(uint8_t *p, uint16_t value) {
	p[0] = (uint8_t)value;
	p[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *p, uint32_t value) {
	p[0] = (uint8_t)value;
	p[1] = (uint8_t)(value >> 8);
	p[2] = (uint8_t)(value >> 16);
	p[3] = (uint8_t)(value >> 24);
}

static int write_sector(uint32_t sector, const uint8_t data[512]) {
	DRESULT result = disk_writep(0, sector);
	if (result != RES_OK) {
		printf("write start failed at sector %lu (result %u)\n",
		       (unsigned long)sector, (unsigned)result);
		return 0;
	}
	result = disk_writep(data, 512);
	if (result != RES_OK) {
		printf("write data failed at sector %lu (result %u)\n",
		       (unsigned long)sector, (unsigned)result);
		return 0;
	}
	result = disk_writep(0, 0);
	if (result != RES_OK) {
		printf("write finalize failed at sector %lu (result %u)\n",
		       (unsigned long)sector, (unsigned)result);
		return 0;
	}
	return 1;
}

static uint8_t probe_write_command(void) {
	static const uint8_t command[] = { 0x58, 0, 0, 0, 0, 0x01 }; /* CMD24, sector 0 */
	SPI_CS_HIGH();
	(void)SPI_TRANSFER_8(0xFF);
	SPI_CS_LOW();
	(void)SPI_TRANSFER_8(0xFF);
	for (unsigned i = 0; i < sizeof(command); ++i)
		(void)SPI_TRANSFER_8(command[i]);
	uint8_t response = 0xFF;
	for (unsigned i = 0; i < 10 && (response & 0x80); ++i)
		response = SPI_TRANSFER_8(0xFF);
	SPI_CS_HIGH();
	(void)SPI_TRANSFER_8(0xFF);
	return response;
}

static uint8_t probe_idle_command(void) {
	static const uint8_t command[] = { 0x40, 0, 0, 0, 0, 0x95 }; /* CMD0 */
	SPI_SET_PRESCALER(7); /* SD startup requires a slow SPI clock. */
	SPI_CS_HIGH();
	for (unsigned i = 0; i < 10; ++i) (void)SPI_TRANSFER_8(0xFF);
	SPI_CS_LOW();
	(void)SPI_TRANSFER_8(0xFF);
	for (unsigned i = 0; i < sizeof(command); ++i)
		(void)SPI_TRANSFER_8(command[i]);
	uint8_t response = 0xFF;
	for (unsigned i = 0; i < 10 && (response & 0x80); ++i)
		response = SPI_TRANSFER_8(0xFF);
	SPI_CS_HIGH();
	(void)SPI_TRANSFER_8(0xFF);
	SPI_SET_PRESCALER(0);
	return response;
}

/* Create an MBR-partitioned, 256 MiB FAT16 volume at LBA 2048. */
static int format_sd_card(void) {
	uint8_t sector[512];

	printf("Formatting SD card as FAT16 (existing data will be erased)...\n");
	if (disk_initialize() & STA_NOINIT) {
		printf("SD card initialization failed\n");
		return 0;
	}
	printf("SD card initialized\n");
	uint8_t write_probe = probe_write_command();
	printf("CMD24 write probe response: 0x%02X\n", write_probe);
	DRESULT probe_result = disk_readp(sector, 0, 510, 2);
	if (probe_result != RES_OK) {
		printf("SD sector 0 read failed (result %u)\n", (unsigned)probe_result);
		return 0;
	}
	printf("SD sector 0 read OK; current signature %02X%02X\n",
	       sector[1], sector[0]);

	/* Master Boot Record with one FAT16 partition. */
	memset(sector, 0, sizeof(sector));
	uint8_t *partition = &sector[446];
	partition[1] = 0xFF; partition[2] = 0xFF; partition[3] = 0xFF;
	partition[4] = 0x06;
	partition[5] = 0xFF; partition[6] = 0xFF; partition[7] = 0xFF;
	put_u32(&partition[8], FORMAT_PARTITION_START);
	put_u32(&partition[12], FORMAT_PARTITION_SECTORS);
	sector[510] = 0x55; sector[511] = 0xAA;
	if (!write_sector(0, sector)) return 0;

	/* FAT16 volume boot record. */
	memset(sector, 0, sizeof(sector));
	sector[0] = 0xEB; sector[1] = 0x3C; sector[2] = 0x90;
	memcpy(&sector[3], "CH582   ", 8);
	put_u16(&sector[11], 512);
	sector[13] = FAT16_SECTORS_PER_CLUSTER;
	put_u16(&sector[14], 1);
	sector[16] = 2;
	put_u16(&sector[17], 512);
	put_u16(&sector[19], 0);
	sector[21] = 0xF8;
	put_u16(&sector[22], FAT16_SECTORS_PER_FAT);
	put_u16(&sector[24], 32);
	put_u16(&sector[26], 64);
	put_u32(&sector[28], 0);
	put_u32(&sector[32], FORMAT_PARTITION_SECTORS);
	sector[36] = 0x80;
	sector[38] = 0x29;
	put_u32(&sector[39], 0x5832A55A);
	memcpy(&sector[43], "CH582 SD   ", 11);
	memcpy(&sector[54], "FAT16   ", 8);
	sector[510] = 0x55; sector[511] = 0xAA;
	if (!write_sector(FORMAT_PARTITION_START, sector)) return 0;

	/* Initialize both FAT copies; mark the media and end-of-chain entries. */
	for (uint32_t fat = 0; fat < 2; ++fat) {
		uint32_t fat_start = FORMAT_PARTITION_START + 1 + fat * FAT16_SECTORS_PER_FAT;
		for (uint32_t i = 0; i < FAT16_SECTORS_PER_FAT; ++i) {
			memset(sector, 0, sizeof(sector));
			if (i == 0) {
				sector[0] = 0xF8; sector[1] = 0xFF;
				sector[2] = 0xFF; sector[3] = 0xFF;
			}
			if (!write_sector(fat_start + i, sector)) return 0;
		}
	}

	/* Clear the fixed root directory. */
	memset(sector, 0, sizeof(sector));
	uint32_t root_start = FORMAT_PARTITION_START + 1 + 2 * FAT16_SECTORS_PER_FAT;
	for (uint32_t i = 0; i < FAT16_ROOT_DIR_SECTORS; ++i)
		if (!write_sector(root_start + i, sector)) return 0;

	printf("FAT16 format complete\n");
	return 1;
}

static int report_error(const char *operation, FRESULT result) {
	if (result == FR_OK) return 0;
	printf("%s failed (FatFs result %u)\n", operation, (unsigned)result);
	return 1;
}

static void run_sd_card_demo(void) {
	static const char message[] = "Hello from CH582 SD card example!\r\n";
	char readback[sizeof(message)];
	UINT written = 0;
	UINT bytes_read = 0;
	FRESULT result;

	SPI_INIT();
	SPI_SET_FRAME_SIZE(SPI_FRAME_8BIT);
	funPinMode(SD_CARD_CS_PIN, GPIO_CFGLR_OUT_2Mhz_PP);
	SPI_CS_HIGH();
	Delay_Ms(10); /* Allow the card to finish power-up before its first command. */
	uint8_t cmd0_response = probe_idle_command();
	printf("CMD0 probe response: 0x%02X\n", cmd0_response);
	if (cmd0_response != 0x01) {
		printf("No SD card response; stopping before format\n");
		return;
	}
	if (!format_sd_card()) {
		printf("SD card format failed\n");
		return;
	}

	printf("Mounting SD card...\n");
	result = pf_mount(&filesystem);
	if (report_error("pf_mount", result)) return;

	printf("Writing %s...\n", DEMO_FILE);
	result = pf_open(DEMO_FILE);
	if (report_error("pf_open (write)", result)) return;
	result = pf_write(message, sizeof(message) - 1, &written);
	if (report_error("pf_write", result)) return;
	if (written != sizeof(message) - 1) {
		printf("Short write: %u of %u bytes\n", (unsigned)written,
		       (unsigned)(sizeof(message) - 1));
		return;
	}
	/* Petit FatFs commits the file by calling pf_write with a null buffer. */
	result = pf_write(0, 0, &written);
	if (report_error("pf_write (finalize)", result)) return;

	printf("Reading %s...\n", DEMO_FILE);
	result = pf_open(DEMO_FILE);
	if (report_error("pf_open (read)", result)) return;
	memset(readback, 0, sizeof(readback));
	result = pf_read(readback, sizeof(readback) - 1, &bytes_read);
	if (report_error("pf_read", result)) return;
	readback[bytes_read] = '\0';
	printf("Read %u bytes: %s", (unsigned)bytes_read, readback);
}

int main(void) {
	SystemInit();
	run_sd_card_demo();
	while (1) { }
}
