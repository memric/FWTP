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
#elif defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#elif _RTOS_ //This means LwIP usage
#include "FreeRTOS.h"
#include "task.h"
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#else
#error "OS is not supported!"
#endif

#if defined(ESP_PLATFORM) || _RTOS_
#define FWTP_THREAD_STACK	(2*1024)
#define FWTP_THREAD_PRIO	(tskIDLE_PRIORITY + 2)
#endif

#ifndef PTRACE
#include <stdio.h>
#define PTRACE				printf
#define PTRACE_ERR			printf
#define PTRACE_WRN			printf
#endif

#if defined(__linux__) || (__APPLE__)
static pthread_t hFWTP = NULL;
#elif defined(ESP_PLATFORM) || _RTOS_
static TaskHandle_t hFWTP = NULL;
#endif

static uint8_t FWTP_RX_Buffer[FWTP_RX_MAX_DATA];
static uint8_t FWTP_TX_Buffer[FWTP_TX_MAX_DATA];

#if defined(__linux__) || (__APPLE__)
void* FWTPServerThread(void * argument);
#else
void FWTPServerThread(void * argument);
#endif
uint32_t FWTPPacketParser(uint8_t *p, uint16_t len);
void FWTPAckSend(struct fwtp_hdr* hdr, int sock, struct sockaddr_in* src);

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
#elif defined(ESP_PLATFORM) || _RTOS_
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
	struct sockaddr_in source_addr;
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
				addrLen = sizeof(struct sockaddr);
				recv_len = recvfrom(sock, FWTP_RX_Buffer, sizeof(FWTP_RX_Buffer), 0, ( struct sockaddr* ) &source_addr, &addrLen);

				/*If data is present*/
				if (recv_len > 0)
				{
					if (FWTPPacketParser(FWTP_RX_Buffer, recv_len) == FWTP_ERR_OK)
					{
                        FWTPAckSend((struct fwtp_hdr*) FWTP_RX_Buffer, sock, &source_addr);
					}
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
#elif defined(ESP_PLATFORM) || _RTOS_
	vTaskDelete(NULL);
#endif
}

/**
 *
 * @param p
 * @param len
 */
uint32_t FWTPPacketParser(uint8_t *p, uint16_t len)
{
	if (len < sizeof(struct fwtp_hdr) + 2)
	{
		return FWTP_ERR_SIZE;
	}

	/*Calculate CRC*/
    uint16_t block_crc16 = *((uint16_t *) &p[len-2]);
	uint16_t calc_crc16 = FWTPCRC(p, len-2);
	if (block_crc16 != calc_crc16)
	{
		PTRACE_ERR("Invalid CRC: expected %d, obtained %d\r\n", calc_crc16, block_crc16);
		return FWTP_ERR_CRC;
	}

	struct fwtp_hdr* hdr = (struct fwtp_hdr*) p;

	if (FWTP_HDR_GET_VER(hdr) != FWTP_VER)
	{
		PTRACE_ERR("FWTP unsupported version: supported %d, obtained %d\r\n", FWTP_VER, FWTP_HDR_GET_VER(hdr));
		return FWTP_ERR_VER;
	}

	if (FWTP_HDR_GET_CMD(hdr) == FWTP_CMD_WR)
	{
		/*Check block size*/
		if (hdr->block_size != (len - 2 - sizeof(struct fwtp_hdr)))
		{
			PTRACE_ERR("Incorrect block size\r\n");
			return FWTP_ERR_SIZE;
		}

		/*Write data block*/
		if (hdr->block_size == FWTPBlockWrite(hdr->file_id, hdr->file_size, hdr->block_offset,
				hdr->block_size, &p[sizeof(struct fwtp_hdr)]))
		{
			/*OK*/
		}
		else
		{
			PTRACE_ERR("Block writing error\r\n");
		}
	}
	else
	{
		return FWTP_ERR_CMD;
	}

	return FWTP_ERR_OK;
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
__attribute__((weak))
uint32_t FWTPBlockWrite(uint8_t file_id, uint32_t ttl_fsize, uint32_t offset, uint16_t len, uint8_t *p)
{
	if (file_id != FWTP_MAINSYSTEM_FILE_ID) return 0;

	/* TODO write routine*/
	PTRACE("Block received; File ID: %u; Offset: %u; Size: %u;\r\n", file_id, offset, len);

    (void) ttl_fsize;
    (void) p;

	return len;
}

void FWTPAckSend(struct fwtp_hdr* hdr, int sock, struct sockaddr_in* src)
{
	struct sockaddr_in addr;

	/* set up address to connect to */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(FWTP_SERVER_PORT);
	addr.sin_addr.s_addr = src->sin_addr.s_addr;

	struct fwtp_hdr* tx_hdr = (struct fwtp_hdr*) FWTP_TX_Buffer;

	memcpy(tx_hdr, hdr, sizeof(struct fwtp_hdr));
	tx_hdr->hdr = 0;
	FWTP_HDR_SET_VER(tx_hdr, FWTP_VER);
	FWTP_HDR_SET_CMD(tx_hdr, FWTP_CMD_ACK);
	FWTP_HDR_SET_ATTR(tx_hdr, 0);

	/*Calculate CRC*/
	uint16_t *pblock_crc16 = (uint16_t *) &FWTP_TX_Buffer[sizeof(struct fwtp_hdr)];
	*pblock_crc16 = FWTPCRC(FWTP_TX_Buffer, sizeof(struct fwtp_hdr));

	/* Send to client*/
	if (sendto(sock, FWTP_TX_Buffer, sizeof(struct fwtp_hdr) + 2, 0,
			( struct sockaddr* ) &addr, sizeof(struct sockaddr_in) ) > 0)
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
