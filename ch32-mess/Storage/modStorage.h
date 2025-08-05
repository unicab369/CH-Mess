#include "ch32fun.h"
#include <stdio.h>

// #include "emulator.h"
// #include "hw_spi.h"

// #include "psram.h"


#include "pff.h"
#include "mmcbbp.h"
#include "thing_config.h"

void die( FRESULT rc )
{
	printf( "Failed with rc=%u.\n", rc );
	for (;;);
}

void load_sd_file(uint32_t addr, const char filename[] ) {

	FATFS fatfs; /* File system object */
	UINT br;
	BYTE buff[64];

	printf( "Mounting volume.\n\r" );
	FRESULT rc = pf_mount( &fatfs );
	if ( rc ) return;

	printf( "Opening file \"%s\"\n\r", filename );
	rc = pf_open( filename );
	if ( rc ) return;

	printf( "Loading image into RAM\n\r" );

	uint32_t total_bytes = 0;
	uint8_t cnt = 0;
	const char spinner[] = "/-\\|";

	for ( ;; ) {
		rc = pf_read( buff, sizeof( buff ), &br ); /* Read a chunk of file */
		if ( rc || !br ) break; /* Error or end of file */

		printf( "Read %u bytes\n\r", br );
		printf( "string: %s\n\r", buff );

		// psram_write( addr, buff, br );
		total_bytes += br;
		addr += br;

		if(total_bytes % (16*1024) == 0){
			cnt++;
			// printf("%d kb so far...  ", total_bytes/1024);
			putchar(spinner[cnt%4]);
			putchar('\r');
		}

	}
	if ( rc ) return;
	// printf("\n\rLoaded %d kilobytes.\n\r", total_bytes/1024);
}


void storage_test() {
	load_sd_file( 0, "testfile.txt" );
}