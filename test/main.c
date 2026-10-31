/**
 * @file main.c
 * @brief FWTP server test application (POSIX).
 */

#include "../fwtp.h"
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile sig_atomic_t keep_running = 1;

/**
 * @brief Handles SIGINT/SIGTERM to stop the test loop gracefully.
 * @param[in] signo Received signal number.
 */
static void handle_signal(int signo)
{
    (void) signo;
    keep_running = 0;
}

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    printf("FWTP server test\r\n");

    if (FWTP_Init() != FWTP_ERR_OK)
    {
        fprintf(stderr, "FWTP_Init failed\r\n");
        return EXIT_FAILURE;
    }

    printf("FWTP server running. Press Ctrl+C to stop.\r\n");

    /* FWTP_ServerThread is detached in fwtp.c, so the process must stay
       alive; pause() sleeps until a signal arrives. */
    while (keep_running)
    {
        pause();
    }

    FWTP_Deinit();

    return EXIT_SUCCESS;
}

/**
 * @brief Test override of the FWTP block-write callback.
 * @param[in] file_id   File ID.
 * @param[in] ttl_fsize Total file size in bytes.
 * @param[in] offset    Block offset in bytes.
 * @param[in] len       Block size in bytes.
 * @param[in] p         Pointer to block data.
 * @return Number of bytes written; 0 on error.
 */
uint32_t FWTP_BlockWrite(uint8_t file_id, uint32_t ttl_fsize, uint32_t offset, uint16_t len, const uint8_t *p)
{
    if (file_id != FWTP_MAINSYSTEM_FILE_ID)
    {
        return 0;
    }

    /* write routine... */

    (void) ttl_fsize;
    (void) offset;
    (void) p;

    return len;
}
