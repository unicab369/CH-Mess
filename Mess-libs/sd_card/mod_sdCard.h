#include "ch32fun.h"
#include <stdio.h>

#include "mmc/mmcbbp.h"

FATFS fatfs; /* File system object */
UINT br;
FRESULT rc;

FRESULT mod_sdCard_write(const char filename[], const char data[]) {
	printf("\nMounting volume.\n\r");
	rc = pf_mount(&fatfs);
	printf("rc=%u\n\r", rc);
	if (rc) return;

	printf("Opening file \"%s\"\n\r", filename);
	rc = pf_open(filename);
	printf("rc=%u\n\r", rc);
	if (rc) return;

	rc = pf_write(data, strlen(data), &br);

	if (rc!=FR_OK) {
		printf("write error: %u\n\r", rc);
		return rc;
	}

	rc = pf_write(0, 0, &br);
	if (rc!=FR_OK) {
		printf("write finish error: %u\n\r", rc);
	}

	return rc;
}

FRESULT mod_sdCard_loadFile(const char filename[], uint32_t addr) {
	BYTE buff[64];

	printf("\nMounting volume.\n\r");
	rc = pf_mount(&fatfs);
	printf("rc=%u\n\r", rc);
	if (rc) return;

	printf("Opening file \"%s\"\n\r", filename);
	rc = pf_open(filename);
	printf("rc=%u\n\r", rc);
	if (rc) return;

	uint32_t total_bytes = 0;
	uint8_t cnt = 0;
	const char spinner[] = "/-\\|";

	for (;;) {
		rc = pf_read(buff, sizeof(buff), &br); /* Read a chunk of file */
		if (rc || !br) break; /* Error or end of file */

		printf("Read %u bytes\n\r", br);
		printf("string: %s\n\r", buff);

		total_bytes += br;
		addr += br;

		if(total_bytes % (16*1024) == 0){
			cnt++;
			printf("%d kb so far...  ", total_bytes/1024);
			putchar(spinner[cnt%4]);
			putchar('\r');
		}
	}

	printf("\n\rLoaded %d kilobytes.\n\r", total_bytes/1024);
	
	return rc;
}
