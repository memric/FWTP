/*
 * fwtp.c
 *
 * FirmWare Transfer Protocol
 *
 *  Created on: Oct 8, 2020
 *      Author: Valeriy Chudnikov
 */

#include "fwtp.h"
#include <string.h>
#if defined(__linux__) || (__APPLE__)
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#elif _WIN32

#elif _RTOS_ //This means LwIP usage
#include "FreeRTOS.h"
#include "task.h"
#include "lwip/opt.h"
#if LWIP_SOCKET
#include "lwip/sockets.h"
#include "lwip/sys.h"
#endif
#else
#error "OS is not supported!"
#endif

#if _RTOS_
#define FWTP_THREAD_STACK	(128*4)
#define FWTP_THREAD_PRIO	(tskIDLE_PRIORITY + 2)
#endif

#ifndef PTRACE
#include <stdio.h>
#define PTRACE				printf
#define PTRACE_ERR			printf
#define PTRACE_WRN			printf
#endif

//#ifndef ARR_2_U16
//#define ARR_2_U16(a)  		(uint16_t) (*(a) << 8) | *( (a)+1 )
//#endif
//#ifndef U16_2_ARR
//#define U16_2_ARR(b,a)  	*(a) = (uint8_t) ( (b) >> 8 ); *(a+1) = (uint8_t) ( (b) & 0xff )
//#endif

#if defined(__linux__) || (__APPLE__)
static pthread_t hFWTP = NULL;
#elif _WIN32
#elif _RTOS_
static TaskHandle_t hFWTP = NULL;
#endif

static uint8_t FWTP_RX_Buffer[FWTP_RX_MAX_DATA];
static uint8_t FWTP_TX_Buffer[FWTP_TX_MAX_DATA];

#if defined(__linux__) || (__APPLE__)
void* FWTPServerThread(void * argument);
#else
void FWTPServerThread(void * argument);
#endif
void FWTPPacketParser(uint8_t *p, uint32_t len, int sock, struct sockaddr* src);
uint32_t FWTPBlockWrite(uint8_t file_id, uint32_t ttl_fsize, uint32_t offset, uint16_t len, uint8_t *p);
void FWTPAckSend(struct fwtp_hdr* hdr, int sock, struct sockaddr* src);

/**
 * @brief FWTP Initialization
 * @return Error code
 */
uint32_t FWTP_Init(void)
{
	if (hFWTP != NULL) return 1;

#if defined(__linux__) || (__APPLE__)
	pthread_create(&hFWTP, NULL, FWTPServerThread, NULL);
	if (pthread_join(hFWTP, NULL)) return 1;
#elif _WIN32
#elif _RTOS_
	xTaskCreate(FWTPServerThread, "FWTP Thread", FWTP_THREAD_STACK, NULL, FWTP_THREAD_PRIO, &hFWTP);
#endif

	return 0;
}

/**
 * @brief Main FWTP thread
 * @param argument
 */
#if defined(__linux__) || (__APPLE__)
void* FWTPServerThread(void * argument)
#else
void FWTPServerThread(void * argument)
#endif
{
	int32_t err, recv_len;
	struct sockaddr source_addr;
	socklen_t addrLen;

	(void) argument;

	PTRACE("Creating new FWTP server\r\n");

	/*Create new socket*/
	int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (sock != -1)
	{
		struct sockaddr_in addr;
		/* set up address to connect to */
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(FWTP_SERVER_PORT);
		addr.sin_addr.s_addr = htonl(INADDR_ANY);

		/* Bind connection to port UDP_PORT. */
		err = bind(sock, (struct sockaddr *) &addr, sizeof(addr));

		if (err == 0)
		{
			while (1)
			{
				recv_len = recvfrom(sock, FWTP_RX_Buffer, sizeof(FWTP_RX_Buffer), 0, ( struct sockaddr* ) &source_addr, &addrLen);

				/*If data is present*/
				if (recv_len > 0)
				{
					FWTPPacketParser(FWTP_RX_Buffer, recv_len, sock, &source_addr);
				}
			}

			close(sock);
		}
		else
		{
			PTRACE_ERR("Can't bind FWTP server to port %d\r\n", FWTP_SERVER_PORT);
		}
	}
	else
	{
		PTRACE_ERR("Can't to create FWTP server\r\n");
	}

#if defined(__linux__) || (__APPLE__)
	return NULL;
#elif _RTOS_
	osThreadTerminate(NULL);
#endif
}

/**
 *
 * @param p
 * @param len
 */
void FWTPPacketParser(uint8_t *p, uint32_t len, int sock, struct sockaddr* src)
{
	if (len < sizeof(struct fwtp_hdr) + 2) return;

	/*Calculate CRC*/
    uint16_t block_crc16 = *((uint16_t *) &p[len-2]);
	uint16_t calc_crc16 = FWTPCRC(p, len-2);
	if (block_crc16 != calc_crc16)
	{
		PTRACE_ERR("Invalid CRC: expected %d, obtained %d\r\n", calc_crc16, block_crc16);
		return;
	}

	struct fwtp_hdr* hdr = (struct fwtp_hdr*) p;

	if (FWTP_HDR_GET_VER(hdr) != FWTP_VER)
	{
		PTRACE_ERR("FWTP unsupported version: supported %d, obtained %d\r\n", FWTP_VER, FWTP_HDR_GET_VER(hdr));
		return;
	}

	if (FWTP_HDR_GET_CMD(hdr) == FWTP_CMD_WR)
	{
		/*Check block size*/
		if (hdr->block_size != (len - 2 - sizeof(struct fwtp_hdr)))
		{
			PTRACE_ERR("Incorrect block size\r\n");
			return;
		}

		/*Write data block*/
		if (hdr->block_size == FWTPBlockWrite(hdr->file_id, hdr->file_size, hdr->block_offset,
				hdr->block_size, &p[sizeof(struct fwtp_hdr)]))
		{
			/*Send acknowledge*/
			FWTPAckSend(hdr, sock, src);
		}
		else
		{
			PTRACE_ERR("Block writing error\r\n");
		}
	}
}

/**
 * @brief Writes file block to file system or memory corresponding to file_id
 * @param file_id
 * @param ttl_fsize
 * @param offset
 * @param len
 * @param p
 * @return
 */
uint32_t FWTPBlockWrite(uint8_t file_id, uint32_t ttl_fsize, uint32_t offset, uint16_t len, uint8_t *p)
{
	if (file_id != FWTP_MAINSYSTEM_FILE_ID) return 0;

	/* TODO write routine*/
	PTRACE("Block received; File ID: %u; Offset: %u; Size: %u;\r\n", file_id, offset, len);

    (void) ttl_fsize;
    (void) p;

	return len;
}

void FWTPAckSend(struct fwtp_hdr* hdr, int sock, struct sockaddr* src)
{
	struct fwtp_hdr* tx_hdr = (struct fwtp_hdr*) FWTP_TX_Buffer;

	memcpy(tx_hdr, hdr, sizeof(struct fwtp_hdr));
	FWTP_HDR_SET_CMD(tx_hdr, FWTP_CMD_ACK);

	/*Calculate CRC*/
	uint16_t *pblock_crc16 = (uint16_t *) &FWTP_TX_Buffer[sizeof(struct fwtp_hdr)];
	*pblock_crc16 = FWTPCRC(FWTP_TX_Buffer, sizeof(struct fwtp_hdr));

	/* Send to client*/
	if (sendto(sock, FWTP_TX_Buffer, sizeof(struct fwtp_hdr) + 2, 0,
			src, sizeof( struct sockaddr )) > 0)
	{
		/*OK*/
	}
	else
	{
		PTRACE_ERR("Acknowledge sending error: socket %i\r\n", sock);
	}
}

/**
 * @brief CRC calculation
 * @param pdata
 * @param len
 * @return
 */
uint16_t FWTPCRC(uint8_t * pdata, uint16_t len)
{
	uint16_t crc = 0xFFFF;
	uint8_t i;

	while (len--)
	{
		crc ^= *pdata++ << 8;

		for (i = 0; i < 8; i++)
			crc = crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1;
	}
	return crc;
}
