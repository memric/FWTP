#!/usr/local/bin/python2.7
# encoding: utf-8
'''
fwtp -- shortdesc

fwtp is a description

It defines classes_and_methods

@author:     user_name

@copyright:  2021 organization_name. All rights reserved.

@license:    license

@contact:    user_email
@deffield    updated: Updated
'''

import sys
import os
import numpy as np

from argparse import ArgumentParser
from argparse import RawDescriptionHelpFormatter
from os.path import getsize

__all__ = []
__version__ = 0.1
__date__ = '2021-04-05'
__updated__ = '2021-04-05'

DEBUG = 0
TESTRUN = 1
PROFILE = 0

FWTP_HDR_SIZE = 14
FWTP_VER = 1

FWTP_CMD_NOPE = 0
# FWTP_CMD_ACK           1 /*Acknowledge*/
# FWTP_CMD_RD                    2 /*Read command (reserved)*/
# FWTP_CMD_WR                    3 /*Write command*/
# FWTP_CMD_START                4 /*Transaction start*/
# FWTP_CMD_STOP                5 /*Transaction stop*/
# FWTP_CMD_CRC                6 /*CRC sending*/
# FWTP_CMD_ERR                7 /*Error*/

class CLIError(Exception):
    '''Generic exception to raise and log different fatal errors.'''
    def __init__(self, msg):
        super(CLIError).__init__(type(self))
        self.msg = "E: %s" % msg
    def __str__(self):
        return self.msg
    def __unicode__(self):
        return self.msg

def getCRC(data):
    crc = np.uint16(0xFFFF)
    pdata = np.uint8(data)
    size = len(pdata)
    for i in range(size):
        crc ^= np.uint16(pdata[i] << 8)
        for j in range(8):
            if (crc & 0x8000) > 0:
                crc = np.uint16((crc << 1) ^ 0x1021)
            else:
                crc = np.uint16(crc << 1)
    return np.uint16(crc & 0xFFFF)

def packetSend(cmd, file_id, ttl_size, block_offset, block_size, data):
    packet = bytearray([FWTP_VER << 6 | cmd << 3]) #set header with version and command
    packet.append(file_id)
    packet += (1).to_bytes(2,'little')
    packet += ttl_size.to_bytes(4,'little')
    packet += block_offset.to_bytes(4,'little')
    packet += block_size.to_bytes(2,'little')
    if data != None:
        packet.append(data)
        
    crc = getCRC(packet)
    packet += int(crc).to_bytes(2,'little')

def main(argv=None): # IGNORE:C0111
    '''Command line options.'''

    if argv is None:
        argv = sys.argv
    else:
        sys.argv.extend(argv)

    program_name = os.path.basename(sys.argv[0])
    program_version = "v%s" % __version__
    program_build_date = str(__updated__)
    program_version_message = '%%(prog)s %s (%s)' % (program_version, program_build_date)
    program_shortdesc = __import__('__main__').__doc__.split("\n")[1]
    program_license = '''%s

  Created by user_name on %s.
  Copyright 2021 organization_name. All rights reserved.

  Distributed on an "AS IS" basis without warranties
  or conditions of any kind, either express or implied.

USAGE
''' % (program_shortdesc, str(__date__))

    try:
        # Setup argument parser
        parser = ArgumentParser(description=program_license, formatter_class=RawDescriptionHelpFormatter)
        parser.add_argument("-a", "--action", dest="act", default='send', help="action [default: %(default)s]")
        parser.add_argument("-i", "--id", dest="id", default=0x10, help="file ID [default: %(default)s]")
        parser.add_argument("-s", "--server", dest="ip", default='127.0.0.1', help="server ip address [default: %(default)s]")
        parser.add_argument("-b", "--block", dest="block", default=512, help="set block size [default: %(default)s]")
        parser.add_argument("-t", "--timeout", dest="tim", default=1000, help="timeout in ms on block sending. [default: %(default)s]" )
        parser.add_argument('-V', '--version', action='version', version=program_version_message)
        parser.add_argument(dest="path", help="paths to source file(s) [default: %(default)s]", metavar="path")

        # Process arguments
        args = parser.parse_args()

        path = args.path
        action = args.act
        file_id = args.id
        block = args.block
        
        with open(str(path), "rb") as f:
            file_size = sys.getsizeof(f)
            print("File %s with %s bytes is opened"%(path, file_size))
            
            packetSend(cmd=FWTP_CMD_NOPE, file_id=file_id, ttl_size=file_size, block_offset=0, block_size=block, data=None)

    except KeyboardInterrupt:
        ### handle keyboard interrupt ###
        return 0
    except Exception as e:
        if DEBUG or TESTRUN:
            raise(e)
        indent = len(program_name) * " "
        sys.stderr.write(program_name + ": " + repr(e) + "\n")
        sys.stderr.write(indent + "  for help use --help")
        return 2

if __name__ == "__main__":
    if DEBUG:
        sys.argv.append("-h")
        sys.argv.append("-V")
    if TESTRUN:
        import doctest
        doctest.testmod()
    if PROFILE:
        import cProfile
        import pstats
        profile_filename = 'fwtp_profile.txt'
        cProfile.run('main()', profile_filename)
        statsfile = open("profile_stats.txt", "wb")
        p = pstats.Stats(profile_filename, stream=statsfile)
        stats = p.strip_dirs().sort_stats('cumulative')
        stats.print_stats()
        statsfile.close()
        sys.exit(0)
    sys.exit(main())