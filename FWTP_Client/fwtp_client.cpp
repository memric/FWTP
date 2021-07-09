#include "fwtp_client.h"
#include <QtNetwork/QHostAddress>
#include "../fwtp.h"
#include <QDebug>
#include <QFile>

#define FWTP_SERVER_PORT	8017
//#define BLOCK_SIZE          512

FWTPClient::FWTPClient(uint8_t id, QString server, QString file, uint16_t block)
{
    server_addr_str = server;
    file_name = file;
    file_id = id;
    block_size = block;

    udp_socket = NULL;
    fw_data = NULL;
    connect(&sending_timer, SIGNAL(timeout()), this, SLOT(TimeoutElapsed()));
    sending_timer.setSingleShot(true);
}

FWTPClient::~FWTPClient()
{

}

void FWTPClient::Start()
{
    QFile FWFile(file_name);

    if (FWFile.open(QIODevice::ReadOnly))
    {
        qDebug() << "File opened. Size: " << FWFile.size();

        auto server_ip = QHostAddress(server_addr_str);

        if (udp_socket == NULL)
        {
            udp_socket = new QUdpSocket(this);
            if (udp_socket->bind(QHostAddress::Any, FWTP_SERVER_PORT) == true)
            {
                connect(udp_socket, SIGNAL(readyRead()), this, SLOT(ReadUDP()));
            }
            else
            {
                qDebug() << "FWTP port is occupied";
            }
        }

        /*Read firmware data from file*/
        if (fw_data != NULL)
            delete fw_data;

        fw_data = new QByteArray;
        fw_data->append(FWFile.readAll());

        FWFile.close();

        /*calculate number of blocks*/
        blocks_num = (fw_data->size() + block_size - 1) / block_size;
        qDebug() << "Total blocks: " <<  blocks_num << " by " << block_size << " bytes";
        if (blocks_num > UINT16_MAX)
        {
            qDebug() << "Too many blocks. Abort";
            return;
        }

        /*Start to send data*/
        state = FILE_START;
        block_id = 0;
        file_offset = 0;
        TimeoutElapsed();
    }
    else
    {
        qDebug() << "Can't open firmware file";
    }
}

void FWTPClient::TimeoutElapsed()
{
    if (state == FILE_START)
    {
        qDebug() << "Start sending for File ID:" << file_id;
        StartWrite(file_id, (uint32_t) fw_data->size());
    }
    else if (state == FILE_SENDING)
    {
        qDebug() << "Block sending:" << block_id << "(" << (int) (100*(block_id+1)/blocks_num) << "% )" ;
        BlockWrite(file_id, (uint32_t) fw_data->size(), file_offset, fw_data->mid(file_offset, block_size));
    }
    else if (state == FILE_STOP)
    {
        qDebug() << "Stop sending";
        StopWrite(file_id);
    }
    sending_timer.start(5000);
}

void FWTPClient::ReadUDP()
{
    while (udp_socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(udp_socket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        udp_socket->readDatagram(datagram.data(), datagram.size(),
                                &sender, &senderPort);

        uint8_t *p = (uint8_t *) datagram.data();

        uint16_t block_crc16 = *((uint16_t *) &p[datagram.size()-2]);
        uint16_t calc_crc16 = FWTPCRC(p, datagram.size()-2);
        if (block_crc16 != calc_crc16)
        {
            qDebug() << "Invalid CRC";
        }

        struct fwtp_hdr* hdr = (struct fwtp_hdr*) p;

        if (FWTP_HDR_GET_CMD(hdr) == FWTP_CMD_ACK)
        {
            sending_timer.stop();

            if (state == FILE_START)
            {
                state = FILE_SENDING;
                TimeoutElapsed();
            }
            else if (state == FILE_SENDING)
            {
                //ui->pbLoad->setValue((int) (100*(block_id+1)/blocks_num));

                block_id++;
                file_offset += block_size;

                if (block_id == blocks_num)
                {
                    state = FILE_STOP;
                }

                TimeoutElapsed();
            }
            else if (state == FILE_STOP)
            {
                state = FILE_FINISHED;
                qDebug() << "Finished";
                emit Finished();
            }
        }
    }
}

uint32_t FWTPClient::BlockWrite(uint8_t file_id, uint32_t ttl_size, uint32_t offset, QByteArray data)
{
    if (data.isEmpty() || (data.size() > UINT16_MAX) || (ttl_size > UINT32_MAX)) {
        return 0;
    }

    QByteArray packet;
    packet.resize(FWTP_HDR_SIZE);
    struct fwtp_hdr* tx_hdr = (struct fwtp_hdr*) packet.data();

    /*Fill header*/
    tx_hdr->hdr = 0;
    FWTP_HDR_SET_VER(tx_hdr, FWTP_VER);
    FWTP_HDR_SET_CMD(tx_hdr, FWTP_CMD_WR);
    tx_hdr->file_id = file_id;
    tx_hdr->file_size = ttl_size;
    tx_hdr->block_offset = offset;
    tx_hdr->block_size = (uint16_t) data.size();
    tx_hdr->packet_id = block_id;

    /*Add data*/
    packet.append(data);

    /*Add CRC*/
    uint16_t crc = FWTPCRC((uint8_t *) packet.data(), (uint16_t) packet.size());
    packet.append((const char *) &crc, 2);

    /*Sending*/
    if (udp_socket->writeDatagram(packet, QHostAddress(server_addr_str), FWTP_SERVER_PORT) < 0)
    {
        qDebug() << "Sending error";
    }

    return (uint32_t) packet.size();
}

uint32_t FWTPClient::StartWrite(uint8_t file_id, uint32_t ttl_size)
{
    if (ttl_size > UINT32_MAX) {
        return 0;
    }

    QByteArray packet;
    packet.resize(FWTP_HDR_SIZE);
    struct fwtp_hdr* tx_hdr = (struct fwtp_hdr*) packet.data();

    /*Fill header*/
    tx_hdr->hdr = 0;
    FWTP_HDR_SET_VER(tx_hdr, FWTP_VER);
    FWTP_HDR_SET_CMD(tx_hdr, FWTP_CMD_START);
    tx_hdr->file_id = file_id;
    tx_hdr->file_size = ttl_size;
    tx_hdr->block_size = 0;
    tx_hdr->packet_id = 0;

    /*Add CRC*/
    uint16_t crc = FWTPCRC((uint8_t *) packet.data(), (uint16_t) packet.size());
    packet.append((const char *) &crc, 2);

    /*Sending*/
    return (uint32_t) udp_socket->writeDatagram(packet, QHostAddress(server_addr_str), FWTP_SERVER_PORT);
}

uint32_t FWTPClient::StopWrite(uint8_t file_id)
{
    QByteArray packet;
    packet.resize(FWTP_HDR_SIZE);
    struct fwtp_hdr* tx_hdr = (struct fwtp_hdr*) packet.data();

    /*Fill header*/
    tx_hdr->hdr = 0;
    FWTP_HDR_SET_VER(tx_hdr, FWTP_VER);
    FWTP_HDR_SET_CMD(tx_hdr, FWTP_CMD_STOP);
    tx_hdr->file_id = file_id;
    tx_hdr->file_size = 0;
    tx_hdr->block_size = 0;
    tx_hdr->packet_id = 0;

    /*Add CRC*/
    uint16_t crc = FWTPCRC((uint8_t *) packet.data(), (uint16_t) packet.size());
    packet.append((const char *) &crc, 2);

    if (udp_socket == NULL)
    {
        udp_socket = new QUdpSocket(this);
        if (udp_socket->bind(QHostAddress::Any, FWTP_SERVER_PORT) == true)
        {
            //connect(udp_socket, SIGNAL(readyRead()), this, SLOT(ReadUDP()));
        }
        else
        {
            qDebug() << "FWTP port is occupied";
        }
    }

    /*Sending*/
    return (uint32_t) udp_socket->writeDatagram(packet, QHostAddress(server_addr_str), FWTP_SERVER_PORT);
}
