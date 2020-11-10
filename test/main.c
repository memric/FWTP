/*
 * main.c
 *
 *  Created on: Oct 9, 2020
 *      Author: valeriychudnikov
 */

#include <stdio.h>
#include <stdlib.h>
#include "../fwtp.h"

int main( int argc, char *argv[] )
{
	printf("FWTP server test\r\n");

	FWTP_Init();

	return EXIT_SUCCESS;
}

uint32_t FWTPBlockWrite(uint8_t file_id, uint32_t ttl_fsize, uint32_t offset, uint16_t len, uint8_t *p)
{
	if (file_id != FWTP_MAINSYSTEM_FILE_ID) return 0;

	/* TODO write routine*/

    (void) ttl_fsize;
    (void) p;

	return len;
}
