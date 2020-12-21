/*
 * fwtp.h
 *
 *  Created on: Oct 8, 2020
 *      Author: Valeriy Chudnikov
 */

#ifndef PLATFORM_SUPPORT_FWTP_H_
#define PLATFORM_SUPPORT_FWTP_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FWTP_VER					1 /*Supported version*/

#define FWTP_SERVER_PORT			8017
#define FWTP_RX_MAX_DATA			(512 + 16)
#define FWTP_TX_MAX_DATA			64

#define FWTP_BOOTLOADER_FILE_ID		0x01
#define FWTP_MAINSYSTEM_FILE_ID		0x10
#define FWTP_SUBSYSTEM1_FILE_ID		0x21
#define FWTP_SUBSYSTEM2_FILE_ID		0x22
#define FWTP_SUBSYSTEM3_FILE_ID		0x23

#define FWTP_CMD_NOPE				0 /*Nope*/
#define FWTP_CMD_ACK				1 /*Acknowledge*/
#define FWTP_CMD_RD					2 /*Read command (reserved)*/
#define FWTP_CMD_WR					3 /*Write command*/
#define FWTP_CMD_START				4 /*Transaction start*/
#define FWTP_CMD_ERR				7 /*Error*/
#define FWTP_CMD_CRC                8 /*CRC sending*/

#define FWTP_HDR_GET_VER(a)			((a)->hdr >> 6) /*Get version from header*/
#define FWTP_HDR_GET_CMD(a)			(((a)->hdr >> 3) & 0x7) /*Get command from header*/
#define FWTP_HDR_GET_ATTR(a)		((a)->hdr & 0x7) /*Get attributes from header*/
#define FWTP_HDR_SET_VER(a, c)		((a)->hdr |= (c) << 6)
#define FWTP_HDR_SET_CMD(a, c)		((a)->hdr |= (c) << 3)
#define FWTP_HDR_SET_ATTR(a, c)		((a)->hdr |= (c))

#define FWTP_HDR_SIZE               sizeof(struct fwtp_hdr)

#define FWTP_ERR_OK					0
#define FWTP_ERR_SIZE				1
#define FWTP_ERR_CRC				2
#define FWTP_ERR_VER				3
#define FWTP_ERR_CMD				4

/**
 * Packet Structure
 * -----------------------
 * 1 byte:	Header: Version (2 bits) + Command (3 bits) + Attributes (3 bits reserved)
 * -----------------------
 * 1 byte:  File ID
 * -----------------------
 * 2 bytes:	Packet ID (reserved)
 * -----------------------
 * 4 bytes: Total file size
 * -----------------------
 * 4 bytes: Block offset
 * -----------------------
 * 2 bytes: Block size
 * -----------------------
 * N bytes: Data...
 * -----------------------
 * 2 bytes: Block CRC16
 */

struct fwtp_hdr {
	uint8_t hdr;
	uint8_t file_id;
	uint16_t packet_id;
	uint32_t file_size;
	uint32_t block_offset;
    uint16_t block_size;
} __attribute__ ((__packed__));

uint32_t FWTP_Init(void);
uint16_t FWTPCRC(uint8_t * pdata, uint16_t len);
uint32_t FWTPBlockWrite(uint8_t file_id, uint32_t ttl_fsize, uint32_t offset, uint16_t len, uint8_t *p);
uint32_t FWTPFileStart(uint8_t file_id, uint32_t ttl_fsize);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_SUPPORT_FWTP_H_ */
