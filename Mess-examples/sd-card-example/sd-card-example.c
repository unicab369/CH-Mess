#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ch32fun.h"

/* CH582 SPI1 uses PC5 (SCK), PC6 (MOSI), and PC7 (MISO). */
#define SD_CS_PIN GPIO_Pin_4
#define INTF_MMC_CS_HIGH() (GPIOC->BSHR = SD_CS_PIN)
#define INTF_MMC_CS_LOW()  (GPIOC->BCR = SD_CS_PIN)
#include "../../Mess-libs/sd_card/mmc/mmcbbp.h"

#define DEMO_FILE "HELLO.TXT"
static FATFS filesystem;

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

	RCC->APB2PCENR |= RCC_APB2Periph_GPIOC;
	GPIOC->CFGLR &= ~(0xFu << (4 * 4));
	GPIOC->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * 4);
	INTF_MMC_CS_HIGH();
	SPI_init2();

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
