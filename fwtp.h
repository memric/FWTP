/**
 * @file fwtp.h
 * @brief FirmWare Transfer Protocol (FWTP).
 * @details UDP-based protocol for transferring firmware images in blocks.
 *          Byte order for multi-byte fields and CRC is little-endian.
 *
 * Packet structure:
 * | 1 byte: Version (2 bits) + Command (3 bits) + Attributes (3 bits, reserved) |
 * | 1 byte: File ID |
 * | 2 bytes: Packet ID (reserved) |
 * | 4 bytes: Total file size (LE) |
 * | 4 bytes: Block offset (LE) |
 * | 2 bytes: Block size (LE) |
 * | N bytes: Data |
 * | 2 bytes: CRC-16/CCITT (poly 0x1021, init 0xFFFF) over header + data (LE) |
 */

#ifndef PLATFORM_SUPPORT_FWTP_H_
#define PLATFORM_SUPPORT_FWTP_H_

#include <stdint.h>

// clang-format off
#ifdef __cplusplus
extern "C"
{
#endif

#define FWTP_VER                1 /*Supported version*/

#define FWTP_SERVER_PORT        8017
#define FWTP_CLIENT_PORT        8018
#ifndef FWTP_MAX_BLOCK_SIZE
#define FWTP_MAX_BLOCK_SIZE     1024
#endif
#define FWTP_RX_MAX_DATA        (FWTP_MAX_BLOCK_SIZE + 16)
#define FWTP_TX_MAX_DATA        64

#define FWTP_BOOTLOADER_FILE_ID 0x01
#define FWTP_MAINSYSTEM_FILE_ID 0x10
#define FWTP_SUBSYSTEM1_FILE_ID 0x21
#define FWTP_SUBSYSTEM2_FILE_ID 0x22
#define FWTP_SUBSYSTEM3_FILE_ID 0x23

#define FWTP_CMD_NOPE           0 /*Nope*/
#define FWTP_CMD_ACK            1 /*Acknowledge*/
#define FWTP_CMD_RD             2 /*Read command (reserved)*/
#define FWTP_CMD_WR             3 /*Write command*/
#define FWTP_CMD_START          4 /*Transaction start*/
#define FWTP_CMD_STOP           5 /*Transaction stop*/
#define FWTP_CMD_CRC            6 /*CRC sending*/
#define FWTP_CMD_ERR            7 /*Error*/

#define FWTP_ATTR_NONE          0 /*No attributes*/

#define FWTP_HDR_GET_VER(a)     (((a)->hdr >> 6) & 0x3) /*Get version from header*/
#define FWTP_HDR_GET_CMD(a)     (((a)->hdr >> 3) & 0x7) /*Get command from header*/
#define FWTP_HDR_GET_ATTR(a)    ((a)->hdr & 0x7)        /*Get attributes from header*/
#define FWTP_HDR_SET_VER(a, c)  ((a)->hdr = ((a)->hdr & ~(0x3 << 6)) | (((c) & 0x3) << 6))
#define FWTP_HDR_SET_CMD(a, c)  ((a)->hdr = ((a)->hdr & ~(0x7 << 3)) | (((c) & 0x7) << 3))
#define FWTP_HDR_SET_ATTR(a, c) ((a)->hdr = ((a)->hdr & ~(0x7)) | ((c) & 0x7))

#define FWTP_HDR_SIZE           sizeof(struct fwtp_hdr)

#define FWTP_ERR_OK             0x00
#define FWTP_ERR_SIZE           0x01
#define FWTP_ERR_CRC            0x02
#define FWTP_ERR_VER            0x03
#define FWTP_ERR_CMD            0x04
#define FWTP_ERR_FILE_ID        0x05
#define FWTP_ERR_SYS            0x06
#define FWTP_ERR_FS             0x10
#define FWTP_ERR_NET            0x11
#define FWTP_ERR_SUBSYSTEM      0x20

/**
 * Packet Structure
 * -----------------------
 * 1 byte:	Version (2 bits) + Command (3 bits) + Attributes (3 bits reserved)
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

struct fwtp_hdr
{
    uint8_t hdr;
    uint8_t file_id;
    uint16_t packet_id;
    uint32_t file_size;
    uint32_t block_offset;
    uint16_t block_size;
} __attribute__((__packed__));

uint32_t FWTP_Init(void);
uint32_t FWTP_Deinit(void);
uint16_t FWTP_CRC(const uint8_t *pdata, uint16_t len);

/**
 * @brief Writes a file data block. Provide your own implementation to
 *        override the weak default (which accepts FWTP_MAINSYSTEM_FILE_ID only).
 * @param[in] file_id   File ID.
 * @param[in] ttl_fsize Total file size in bytes.
 * @param[in] offset    Block offset in bytes.
 * @param[in] len       Block size in bytes.
 * @param[in] p         Pointer to block data.
 * @return Number of bytes written; 0 on error.
 */
uint32_t FWTP_BlockWrite(uint8_t file_id, uint32_t ttl_fsize, uint32_t offset, uint16_t len, const uint8_t *p);

/**
 * @brief Invoked on START command. Override the weak default as needed.
 * @param[in] file_id   File ID.
 * @param[in] ttl_fsize Total file size in bytes.
 * @return FWTP_ERR_OK on success, otherwise an FWTP_ERR_* code.
 */
uint32_t FWTP_FileStart(uint8_t file_id, uint32_t ttl_fsize);

/**
 * @brief Invoked on STOP command. Override the weak default as needed.
 * @param[in] file_id File ID.
 * @return FWTP_ERR_OK on success, otherwise an FWTP_ERR_* code.
 */
uint32_t FWTP_FileStop(uint8_t file_id);

/**
 * @brief Invoked after the ACK is sent. Override the weak default as needed.
 * @param[in] hdr Pointer to the received packet header.
 */
void FWTP_PendingOperation(struct fwtp_hdr *hdr);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_SUPPORT_FWTP_H_ */
