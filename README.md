# FWTP — FirmWare Transfer Protocol

Lightweight UDP-based protocol for transferring firmware images and data files to embedded devices.

FWTP is intentionally very simple and unsecured: data is sent as raw, uncompressed, unencrypted blocks in plain UDP datagrams. There is no authentication, no encryption, and no protection against eavesdropping, spoofing, or replay attacks — use it only on trusted local networks or behind an additional secure transport.

The repository contains two parts:

- **C server library** (`fwtp.c` / `fwtp.h`) — runs on the target device, receives files over UDP.
- **Python client** (`pyFWTP/`) — sends files to the device from a host PC.

Supported platforms for the C server: Linux, macOS, ESP-IDF (`ESP_PLATFORM`), FreeRTOS + lwIP (`_FREERTOS_`).

## How It Works

1. The client sends a `START` command with the file ID and total file size. On receiving this command the device (server) can allocate memory to receive the data or unlock and clear the flash memory, etc.  
2. The file is split into blocks (up to `FWTP_MAX_BLOCK_SIZE`, default 1024 bytes) and each block is sent with a `WR` command carrying its offset. The device can write each block to storage.
3. Every valid packet is acknowledged by the server with an `ACK` response.
4. The client finishes the transfer with a `STOP` command. The server completes the transfer procedure.
5. A `NOPE` command can be used to check connectivity without side effects.

Each packet is protected with CRC-16-CCITT (`0xFFFF` init, `0x1021` poly), covering header + payload.

Default ports:

- Server (device): UDP `8017`
- Client (host): UDP `8018`

## Packet Format

```text
1 byte:  Version (2 bits) + Command (3 bits) + Attributes (3 bits, reserved)
1 byte:  File ID
2 bytes: Packet ID (reserved)
4 bytes: Total file size
4 bytes: Block offset
2 bytes: Block size
N bytes: Data...
2 bytes: Block CRC-16
```

Total header size: 14 bytes (`sizeof(struct fwtp_hdr)`).

## Commands

| Value | Name    | Description          |
| ----- | ------- | -------------------- |
| 0     | `NOPE`  | No operation (link check) |
| 1     | `ACK`   | Acknowledge (server response) |
| 2     | `RD`    | Read (reserved)      |
| 3     | `WR`    | Write data block     |
| 4     | `START` | Transaction start    |
| 5     | `STOP`  | Transaction stop     |
| 6     | `CRC`   | CRC sending          |
| 7     | `ERR`   | Error                |

## Error Codes

| Value  | Name          |
| ------ | ------------- |
| `0x00` | OK            |
| `0x01` | Size error    |
| `0x02` | CRC error     |
| `0x03` | Version error |
| `0x04` | Command error |
| `0x05` | File ID error |
| `0x10` | Filesystem error |
| `0x11` | Network error |
| `0x20` | Subsystem error |

## C Server Integration

Add `fwtp.c` / `fwtp.h` to your firmware project and start the server:

```c
#include "fwtp.h"

int main(void)
{
    FWTP_Init();
    /* ... */
    FWTP_Deinit();
    return 0;
}
```

Override the weak callbacks with your own storage logic:

```c
uint32_t FWTP_FileStart(uint8_t file_id, uint32_t ttl_fsize);
uint32_t FWTP_FileStop(uint8_t file_id);
uint32_t FWTP_BlockWrite(uint8_t file_id, uint32_t ttl_fsize,
                         uint32_t offset, uint16_t len, const uint8_t *p);
void FWTP_PendingOperation(struct fwtp_hdr *hdr);
```

- `FWTP_FileStart` — called on `START`; prepare to receive `ttl_fsize` bytes for `file_id`, return `FWTP_ERR_OK` on success.
- `FWTP_BlockWrite` — called for each `WR` block; write `len` bytes at `offset`, return the number of bytes written.
- `FWTP_FileStop` — called on `STOP`; finalize/verify the received file.
- `FWTP_PendingOperation` — invoked after the ACK is sent; use for deferred work.

Tunable compile option:

- `FWTP_MAX_BLOCK_SIZE` — maximum data block size (default `1024`).

Header helpers: `FWTP_HDR_GET_VER` / `FWTP_HDR_GET_CMD` / `FWTP_HDR_GET_ATTR`, `FWTP_HDR_SET_VER` / `FWTP_HDR_SET_CMD` / `FWTP_HDR_SET_ATTR`.

## Python Client

Install dependencies ([rich](https://github.com/textualize/rich) is used for console status bar):

```bash
pip install -r pyFWTP/requirements.txt
```

Send a firmware file:

```bash
python pyFWTP/fwtp_client.py firmware.bin -a 192.168.1.10 -i 16 -s 256
```

CLI options:

- `file` — file to send.
- `-a, --addr` — server IP address (default `127.0.0.1`).
- `-i, --id` — file ID (default `0x10`, main system).
- `-s, --size` — block size in bytes (default `256`).
- `-t, --timeout` — ACK wait timeout in seconds (default `8`).

Each `write()` transmits one block and waits for the server ACK. The CLI retries a failed block up to 5 times before aborting.

## Test Server App

Build and run the minimal C server on Linux/macOS:

```bash
cd test
make
./build/FWTP_test
```

Then send it a file with the Python client pointed at `127.0.0.1`.
