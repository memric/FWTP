#!/usr/bin/env python

import sys
from argparse import ArgumentParser
from fwtp import FWTP
from rich.progress import track
from time import sleep
from rich.console import Console


def main(argv=None):
    if argv is None:
        argv = sys.argv
    else:
        sys.argv.extend(argv)

    try:
        # Setup argument parser
        parser = ArgumentParser(description="FWTP client.")
        parser.add_argument("file", help="File to send", metavar="file")
        parser.add_argument(
            "-a", "--addr", dest="ip", default="127.0.0.1", help="Server IP address"
        )
        parser.add_argument("-i", "--id", dest="id", default=0x10, help="File ID")
        parser.add_argument(
            "-s", "--size", dest="block_size", default=256, help="Block size"
        )
        parser.add_argument('-t', '--timeout', dest='timeout', type=int, default=8, help='Waiting timeout pause.')
        parser.add_argument("--version", action="version", version="0.0.1")

        # Process arguments
        args = parser.parse_args()

        file_name = args.file
        file_id = int(args.id)
        ip = args.ip
        block_size = int(args.block_size)

        console = Console()

        try:
            file = open(file_name, "rb")
            file_data = file.read()
            file_len = len(file_data)
            blocks_num = int((file_len + block_size - 1) / block_size)

            console.print(
                "File size: %d; Blocks: %d; Block size %d"
                % (file_len, blocks_num, block_size)
            )
        except:
            console.print("[red]Error. Can't open file")
            return 1

        client = FWTP(ip, timeout=args.timeout)

        # Nope commant to check link
        console.print("Check server on IP: %s -> " % (ip), end="")
        if client.nope() == True:
            console.print("[green]PASSED")
        else:
            console.print("[red]FAILED")
            return 1

        # Start command
        console.print("Start file transfer -> ", end="")
        if client.start(file_id, file_len) == True:
            console.print("[green]OK")
        else:
            console.print("[red]FAILED")
            return 1

        for i in track(range(blocks_num), description="Sending..."):
            # send block
            offset = block_size * i
            curr_block_size = block_size
            if (file_len - offset) < block_size:
                curr_block_size = file_len - offset
            else:
                curr_block_size = block_size

            for _ in range(5):
                if (
                    client.write(
                        file_id,
                        file_len,
                        offset,
                        curr_block_size,
                        file_data[offset : offset + curr_block_size],
                    )
                    == True
                ):
                    break
            else:
                console.print("[red]Block %d sending error" % (i))
                return 1

        # Stop
        console.print("Stop file transfer -> ", end="")
        if client.stop(file_id) == True:
            console.print("[green]OK")
        else:
            console.print("[red]FAILED")
            return 1

        file.close()

        return 0

    except KeyboardInterrupt:
        ### handle keyboard interrupt ###
        return 0
    except Exception as e:
        sys.stderr.write(" for help use --help")
        return 2


if __name__ == "__main__":
    sys.exit(main())
