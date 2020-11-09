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
