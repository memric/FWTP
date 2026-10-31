/**
 * @file fwtp.c
 * @brief FirmWare Transfer Protocol (FWTP) UDP server implementation.
 */

#include "fwtp.h"
#include <inttypes.h>
#include <string.h>
#if defined(__linux__) || (__APPLE__)
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#elif defined(ESP_PLATFORM)
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#elif _FREERTOS_ // This means LwIP usage
#include "FreeRTOS.h"
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "task.h"
#else
#error "OS is not supported!"
#endif

#if defined(ESP_PLATFORM) || _FREERTOS_
#define FWTP_THREAD_STACK (4 * 1024)
#define FWTP_THREAD_PRIO  (tskIDLE_PRIORITY + 2)
#endif

#if defined(ESP_PLATFORM)
#define PTRACE(m...)     ESP_LOGI("FWTP", m)
#define PTRACE_ERR(m...) ESP_LOGE("FWTP", m)
#define PTRACE_WRN(m...) ESP_LOGW("FWTP", m)
#endif

#ifndef PTRACE
#include <stdio.h>
#define PTRACE     printf
#define PTRACE_ERR printf
#define PTRACE_WRN printf
#endif

#if defined(__linux__) || (__APPLE__)
static pthread_t hFWTP = NULL;
#elif defined(ESP_PLATFORM) || _FREERTOS_
static TaskHandle_t hFWTP = NULL;
#endif

static uint8_t FWTP_RX_Buffer[FWTP_RX_MAX_DATA];
static uint8_t FWTP_TX_Buffer[FWTP_TX_MAX_DATA];
static int fwtp_socket = -1;

#if defined(__linux__) || (__APPLE__)
void *FWTP_ServerThread(void *argument);
#else
void FWTP_ServerThread(void *argument);
#endif
uint32_t FWTP_PacketParser(const uint8_t *p, uint16_t len);
uint32_t FWTP_AckSend(struct fwtp_hdr *hdr, int sock, struct sockaddr_in *src);

/**
 * @brief FWTP initialization. Starts the UDP server thread.
 * @return FWTP_ERR_OK on success. FWTP_ERR_SYS on thread creating error.
 */
uint32_t FWTP_Init(void)
{
    if (hFWTP != NULL)
    {
        return FWTP_ERR_SYS;
    }

#if defined(__linux__) || (__APPLE__)
    if (pthread_create(&hFWTP, NULL, FWTP_ServerThread, NULL) != 0)
    {
        return FWTP_ERR_SYS;
    }

    pthread_detach(hFWTP);
#elif defined(ESP_PLATFORM) || _FREERTOS_
    if (xTaskCreate(FWTP_ServerThread, "FWTP Thread", FWTP_THREAD_STACK, NULL, FWTP_THREAD_PRIO, &hFWTP) != pdPASS)
    {
        hFWTP = NULL;
        return FWTP_ERR_SYS;
    }
#endif

    return FWTP_ERR_OK;
}

/**
 * @brief FWTP deinitialization. Stops the server thread and closes the socket.
 * @return FWTP_ERR_OK on success.
 */
uint32_t FWTP_Deinit(void)
{
#if defined(__linux__) || (__APPLE__)
    if (fwtp_socket != -1)
    {
        /* shutdown() unblocks a thread stuck in recvfrom(); close() alone does not. */
        shutdown(fwtp_socket, SHUT_RDWR);
        close(fwtp_socket);
        fwtp_socket = -1;
    }
#elif defined(ESP_PLATFORM) || _FREERTOS_
    if (hFWTP != NULL)
    {
        vTaskDelete(hFWTP);
        hFWTP = NULL;
    }
    
    if (fwtp_socket != -1)
    {
        close(fwtp_socket);
        fwtp_socket = -1;
    }
#endif

    return FWTP_ERR_OK;
}

/**
 * @brief Main FWTP server thread. Creates a UDP socket, binds it to
 *        FWTP_SERVER_PORT and dispatches incoming packets.
 * @param[in] argument Unused thread argument.
 */
#if defined(__linux__) || (__APPLE__)
void *FWTP_ServerThread(void *argument)
#else
void FWTP_ServerThread(void *argument)
#endif
{
    int32_t err;
    ssize_t recv_len;
    struct sockaddr_in source_addr;
    socklen_t addrLen;

    (void) argument;

    PTRACE("Creating a new FWTP server\r\n");

    /* Create new socket */
    fwtp_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (fwtp_socket != -1)
    {
        struct sockaddr_in addr;
        /* set up address to connect to */
        memset(&addr, 0, sizeof(addr));
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons(FWTP_SERVER_PORT);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        /* Bind connection to port UDP_PORT. */
        err = bind(fwtp_socket, (struct sockaddr *) &addr, sizeof(addr));

        if (err == 0)
        {
            while (1)
            {
                addrLen  = sizeof(source_addr);
                recv_len = recvfrom(
                    fwtp_socket,
                    FWTP_RX_Buffer,
                    sizeof(FWTP_RX_Buffer),
                    0,
                    (struct sockaddr *) &source_addr,
                    &addrLen
                );

                /* If data is present, validate, ACK, then run post-ACK work. */
                if (recv_len > 0)
                {
                    if (FWTP_PacketParser(FWTP_RX_Buffer, (uint16_t) recv_len) == FWTP_ERR_OK)
                    {
                        FWTP_AckSend((struct fwtp_hdr *) FWTP_RX_Buffer, fwtp_socket, &source_addr);

                        /* To do after acknowledge */
                        FWTP_PendingOperation((struct fwtp_hdr *) FWTP_RX_Buffer);
                    }
                    else
                    {
                        PTRACE_ERR("FWTP error\r\n");

                        // TODO Is Error need to be sent?
                    }
                }
            }

            if (fwtp_socket != -1)
            {
                close(fwtp_socket);
                fwtp_socket = -1;
            }
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
#elif defined(ESP_PLATFORM) || _FREERTOS_
    vTaskDelete(NULL);
#endif
}

/**
 * @brief Validates an incoming FWTP packet: checks length, CRC-16, version
 *        and dispatches START/STOP/WRITE commands.
 * @note Wire format is little-endian.
 * @param[in] p   Pointer to received datagram.
 * @param[in] len Datagram length in bytes.
 * @return FWTP_ERR_OK on success, otherwise one of FWTP_ERR_* codes.
 */
uint32_t FWTP_PacketParser(const uint8_t *p, uint16_t len)
{
    if (p == NULL)
    {
        return FWTP_ERR_SIZE;
    }

    if ((len < sizeof(struct fwtp_hdr) + 2) || (len > sizeof(FWTP_RX_Buffer)))
    {
        PTRACE_ERR("Invalid packet length\r\n");
        return FWTP_ERR_SIZE;
    }

    /* Calculate CRC */
    uint16_t block_crc16 = *((uint16_t *) &p[len - 2]);
    uint16_t calc_crc16  = FWTP_CRC(p, len - 2);
    if (block_crc16 != calc_crc16)
    {
        PTRACE_ERR("Invalid CRC: expected %u, obtained %u\r\n", calc_crc16, block_crc16);
        return FWTP_ERR_CRC;
    }

    struct fwtp_hdr *hdr = (struct fwtp_hdr *) p;

    if (FWTP_HDR_GET_VER(hdr) != FWTP_VER)
    {
        PTRACE_ERR("FWTP unsupported version: supported %d, obtained %d\r\n", FWTP_VER, FWTP_HDR_GET_VER(hdr));
        return FWTP_ERR_VER;
    }

    /*Check command*/
    if (FWTP_HDR_GET_CMD(hdr) == FWTP_CMD_NOPE)
    {
        PTRACE("Nope command received\r\n");
        return FWTP_ERR_OK;
    }
    else if (FWTP_HDR_GET_CMD(hdr) == FWTP_CMD_START)
    {
        return FWTP_FileStart(hdr->file_id, hdr->file_size);
    }
    else if (FWTP_HDR_GET_CMD(hdr) == FWTP_CMD_STOP)
    {
        return FWTP_FileStop(hdr->file_id);
    }
    else if (FWTP_HDR_GET_CMD(hdr) == FWTP_CMD_WR)
    {
        /* Check block size */
        if (hdr->block_size != (len - 2 - sizeof(struct fwtp_hdr)) || hdr->block_size == 0)
        {
            PTRACE_ERR("Incorrect block size\r\n");
            return FWTP_ERR_SIZE;
        }

        /* Write data block */
        if (hdr->block_size != FWTP_BlockWrite(
                                   hdr->file_id,
                                   hdr->file_size,
                                   hdr->block_offset,
                                   hdr->block_size,
                                   &p[sizeof(struct fwtp_hdr)]))
        {
            PTRACE_ERR("Block writing error\r\n");
            return FWTP_ERR_FS;
        }
    }
    else
    {
        return FWTP_ERR_CMD;
    }

    return FWTP_ERR_OK;
}

/**
 * @brief           Callback function. Invoked to write a file data block to the file system or memory.
 *                  Provide your own implementation.
 * @param file_id   File ID
 * @param ttl_fsize Total file size
 * @param offset    Block offset
 * @param len       Block size
 * @param p         Pointer to block data
 * @return          Number of written bytes
 */
__attribute__((weak)) uint32_t
FWTP_BlockWrite(uint8_t file_id, uint32_t ttl_fsize, uint32_t offset, uint16_t len, const uint8_t *p)
{
    if (file_id != FWTP_MAINSYSTEM_FILE_ID)
    {
        return 0;
    }

    /* TODO write routine*/
    PTRACE("Block received; File ID: %" PRIu8 "; Offset: %" PRIu32 "; Size: %" PRIu16 ";\r\n", file_id, offset, len);

    (void) ttl_fsize;
    (void) p;

    return len;
}

/**
 * @brief           Callback function. Invoked when start command received.
 *                  Provide your own implementation.
 * @param file_id   File ID
 * @param ttl_fsize Total file size
 * @return          Error code. FWTP_ERR_OK for success.
 */
__attribute__((weak)) uint32_t FWTP_FileStart(uint8_t file_id, uint32_t ttl_fsize)
{
    if (file_id != FWTP_MAINSYSTEM_FILE_ID)
    {
        return FWTP_ERR_SUBSYSTEM;
    }

    PTRACE("File start command received; File ID: %" PRIu8 "; Size: %" PRIu32 "\r\n", file_id, ttl_fsize);

    return FWTP_ERR_OK;
}

/**
 * @brief           Callback function. Invoked when stop command received.
 *                  Provide your own implementation.
 * @param file_id   File ID
 * @return          Error code. FWTP_ERR_OK for success.
 */
__attribute__((weak)) uint32_t FWTP_FileStop(uint8_t file_id)
{
    if (file_id != FWTP_MAINSYSTEM_FILE_ID)
    {
        return FWTP_ERR_SUBSYSTEM;
    }

    PTRACE("File stop command received; File ID: %u\r\n", file_id);

    return FWTP_ERR_OK;
}

/**
 * @brief Sends an ACK response for a received packet header.
 * @param[in] hdr  Pointer to the received packet header (copied into ACK).
 * @param[in] sock UDP socket to send from.
 * @param[in] src  Source address of the request; reply goes to its IP on FWTP_CLIENT_PORT.
 * @return FWTP_ERR_OK on success, FWTP_ERR_NET on send failure.
 */
uint32_t FWTP_AckSend(struct fwtp_hdr *hdr, int sock, struct sockaddr_in *src)
{
    if ((hdr == NULL) || (src == NULL) || (sock < 0))
    {
        return FWTP_ERR_NET;
    }

    struct sockaddr_in addr;

    /* set up address to connect to */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(FWTP_CLIENT_PORT);
    addr.sin_addr.s_addr = src->sin_addr.s_addr;

    struct fwtp_hdr *tx_hdr = (struct fwtp_hdr *) FWTP_TX_Buffer;

    memcpy(tx_hdr, hdr, sizeof(struct fwtp_hdr));
    tx_hdr->hdr = 0;
    FWTP_HDR_SET_VER(tx_hdr, FWTP_VER);
    FWTP_HDR_SET_CMD(tx_hdr, FWTP_CMD_ACK);
    FWTP_HDR_SET_ATTR(tx_hdr, FWTP_ATTR_NONE);

    /*Calculate CRC*/
    uint16_t *pblock_crc16 = (uint16_t *) &FWTP_TX_Buffer[sizeof(struct fwtp_hdr)];
    *pblock_crc16          = FWTP_CRC(FWTP_TX_Buffer, sizeof(struct fwtp_hdr));

    /* Send to client*/
    if (sendto(
            sock,
            FWTP_TX_Buffer,
            sizeof(struct fwtp_hdr) + 2,
            0,
            (struct sockaddr *) &addr,
            sizeof(struct sockaddr_in)
        ) <= 0)
    {
        PTRACE_ERR("Acknowledge sending error: socket %i\r\n", sock);
        return FWTP_ERR_NET;
    }

    return FWTP_ERR_OK;
}

/**
 * @brief Invoked after the ACK has been sent. Default implementation does nothing.
 *        Override to trigger post-packet work (e.g. reboot after STOP).
 * @param[in] hdr Pointer to the received packet header.
 */
__attribute__((weak)) void FWTP_PendingOperation(struct fwtp_hdr *hdr)
{
    (void) hdr;
}

/**
 * @brief Calculates CRC-16/CCITT (poly 0x1021, init 0xFFFF) over a buffer.
 * @param[in] pdata Pointer to data.
 * @param[in] len   Number of bytes to process.
 * @return Computed CRC-16 value (host byte order; wire order is little-endian).
 */
uint16_t FWTP_CRC(const uint8_t *pdata, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    uint8_t i;

    while (len--)
    {
        crc ^= *pdata++ << 8;

        for (i = 0; i < 8; i++)
        {
            crc = crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1;
        }
    }

    return crc;
}
