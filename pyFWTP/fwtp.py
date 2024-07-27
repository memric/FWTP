import numpy as np
import struct
import array
import socket

FWTP_SERVER_PORT = 8017
FWTP_CLIENT_PORT = 8018
FWTP_HDR_SIZE = 14
FWTP_VER = 1

FWTP_MAX_BLOCK_SIZE = 1024
FWTP_RX_MAX_DATA = FWTP_MAX_BLOCK_SIZE + 16

# Commands
FWTP_CMD_NOPE = 0  # Do nothing
FWTP_CMD_ACK = 1  # Acknowledge
FWTP_CMD_RD = 2  # Read(reserved)
FWTP_CMD_WR = 3  # Write block
FWTP_CMD_START = 4  # Transaction start
FWTP_CMD_STOP = 5  # Transaction stop
FWTP_CMD_CRC = 6  # CRC sending
FWTP_CMD_ERR = 7  # Error


class FWTP:

    def __init__(self, client, timeout=8):
        self.client = client
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("", FWTP_CLIENT_PORT))
        self.sock.settimeout(timeout)

    # Nope command request
    def nope(self):
        packet = self.pack(FWTP_CMD_NOPE)
        resp = self.send_receive(packet)

        if resp != None:
            payload = self.unpack(resp)
            if payload != None:
                return True

        return False

    # Start command
    def start(self, file_id, ttl_size):
        packet = self.pack(FWTP_CMD_START, file_id=file_id, ttl_size=ttl_size)
        resp = self.send_receive(packet)

        if resp != None:
            payload = self.unpack(resp)
            if payload != None:
                return True

        return False

    # Stop command
    def stop(self, file_id):
        packet = self.pack(FWTP_CMD_STOP, file_id=file_id)
        resp = self.send_receive(packet)

        if resp != None:
            payload = self.unpack(resp)
            if payload != None:
                return True

        return False

    # Send block
    def write(self, file_id, ttl_size, block_offset, block_size, data):
        packet = self.pack(
            FWTP_CMD_WR, file_id, ttl_size, block_offset, block_size, data
        )
        resp = self.send_receive(packet)

        if resp != None:
            payload = self.unpack(resp)
            if payload != None:
                return True

        return False

    # Packet composer
    def pack(self, cmd, file_id=0, ttl_size=0, block_offset=0, block_size=0, data=None):
        # Construct header
        packet = struct.pack(
            "<BBHLLH",
            FWTP_VER << 6 | cmd << 3,  # Set version and command
            file_id,  # File ID
            1,  # Packet ID
            ttl_size,  # Total file size
            block_offset,  # Block offset
            block_size,  # Block size
        )

        # add data
        if data != None:
            packet += data

        # CRC calculation
        crc = self.getCRC(packet)
        packet += struct.pack("<H", crc)

        return packet

    # Packet parser
    def unpack(self, data):
        # check packet size
        if len(data) < (FWTP_HDR_SIZE + 2):
            print("Incorrect packet size")  # TODO remove
            return None

        # check packet CRC
        recv_crc = struct.unpack("<H", data[-2:])
        calc_crc = self.getCRC(data[:-2])
        if recv_crc != calc_crc:
            print("Incorrect CRC")  # TODO remove
            return None

        # get header values
        header = data[:FWTP_HDR_SIZE]
        hdr_params = struct.unpack("<BBHLLH", header)
        #        print(hdr_params)

        # get version
        if (hdr_params[0] >> 6) != FWTP_VER:
            print("Incorrect version")  # TODO remove
            return None

        # get command
        if ((hdr_params[0] >> 3) & 0x7) != FWTP_CMD_ACK:
            print("Is not acknowledge command")  # TODO remove
            return None

        # return payload
        return data[FWTP_HDR_SIZE:]

    # Sends packet to server and receives response
    def send_receive(self, packet):
        self.sock.sendto(packet, (self.client, FWTP_SERVER_PORT))
        try:
            data, src = self.sock.recvfrom(FWTP_RX_MAX_DATA)
            # TODO Check src
            return data
        except:
            return None

    def getCRC(self, data):
        crc = np.uint16(0xFFFF)
        pdata = array.array("B", data)
        size = len(pdata)
        for i in range(size):
            crc ^= np.uint16(pdata[i] << 8)
            for j in range(8):
                if (crc & 0x8000) > 0:
                    crc = np.uint16((crc << 1) ^ 0x1021)
                else:
                    crc = np.uint16(crc << 1)
        return np.uint16(crc & 0xFFFF)
