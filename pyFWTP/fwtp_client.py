#!/usr/bin/env python

import sys
from argparse import ArgumentParser
from fwtp import FWTP
from tqdm import tqdm
from time import sleep

def main(argv=None):
    if argv is None:
        argv = sys.argv
    else:
        sys.argv.extend(argv)
    
    try:
        # Setup argument parser
        parser = ArgumentParser(description = 'FWTP client.')
        parser.add_argument('file', help='File to send', metavar='file')
        parser.add_argument('-a', '--addr', dest='ip', default='127.0.0.1', help='Server IP address')
        parser.add_argument('-i', '--id', dest='id', default=0x10, help='File ID')
        parser.add_argument('-s', '--size', dest='block_size', default=256, help='Block size')
#        parser.add_argument('-p', '--pause', dest='pause', default=100, help='Cycle pause')
        parser.add_argument('--version', action='version', version='0.0.1')
        
        # Process arguments
        args = parser.parse_args()
        
        file_name = args.file
        file_id = int(args.id)
        ip = args.ip
        block_size = int(args.block_size)
        
        try:
            file = open(file_name, 'rb')
            file_data = file.read()
            file_len = len(file_data)
            blocks_num = int((file_len + block_size - 1) / block_size)
            
            print('File size: %d; Blocks: %d; Block size %d'%(file_len, blocks_num, block_size))
        except:
            print("Can't open file")
            return 1
        
        client = FWTP(ip)
        
        # Nope commant to check link
        print('Check server on IP: %s -> '%(ip), end='')
        if client.nope() == True:
            print('PASSED')
        else:
            print('FAILED')
            return 1
            
        # Start command
        print('Start file transfer -> ', end='')
        if client.start(file_id, file_len) == True:
            print('OK')
        else:
            print('FAILED')
            return 1

        for i in tqdm(range(blocks_num)):
            #send block
            offset = block_size*i
            curr_block_size = block_size
            if (file_len - offset) < block_size:
                curr_block_size = file_len - offset
            else:
                curr_block_size = block_size
                
#            tqdm.write('Block %d; Offset %d; Size %d'%(i, offset, curr_block_size))
            
            if client.write(file_id, file_len, offset, curr_block_size, file_data[offset:offset+curr_block_size]) == False:
                tqdm.write('Block %d sending error'%(i))
                return 1
                
#            sleep(0.1)

        # Stop
        print('Stop file transfer -> ', end='')
        if client.stop(file_id) == True:
            print('OK')
        else:
            print('FAILED')
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

