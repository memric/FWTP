#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QtNetwork/QHostAddress>
#include "../fwtp.h"

#define FWTP_SERVER_PORT	8017
#define BLOCK_SIZE          512

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    udp_socket = NULL;
    fw_data = NULL;
    connect(&sending_timer, SIGNAL(timeout()), this, SLOT(TimeoutElapsed()));
    sending_timer.setSingleShot(true);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_pbFile_clicked()
{
    ui->leFile->setText(QFileDialog::getOpenFileName(this, tr("Open Firmware File"),
                                                            "fw.bin",
                                                            tr("Binary File (*.bin *.img)")));
}

void MainWindow::on_pbStart_clicked()
{
    if (ui->pbStart->text() == "Stop")
    {
        sending_timer.stop();
        ui->pbStart->setText("Start");
        return;
    }

    ui->peStatus->clear();

    if (ui->leFile->text().isEmpty())
    {
        ui->peStatus->appendPlainText("No input file");
    }

    QFile FWFile(ui->leFile->text());

    if (FWFile.open(QIODevice::ReadOnly))
    {
        ui->peStatus->appendPlainText(tr("File opened. Size: %1").arg(FWFile.size()));

        auto server_ip = QHostAddress(ui->leServer->text());

        if (udp_socket == NULL)
        {
            udp_socket = new QUdpSocket(this);
            if (udp_socket->bind(QHostAddress::LocalHost, FWTP_SERVER_PORT+1) == true)
            {
                connect(udp_socket, SIGNAL(readyRead()), this, SLOT(ReadUDP()));
            }
            else
            {
                ui->peStatus->appendPlainText("FWTP port is occupied");
            }
        }

        /*Read firmware data from file*/
        if (fw_data != NULL)
            delete fw_data;

        fw_data = new QByteArray;
        fw_data->append(FWFile.readAll());

        FWFile.close();

        /*calculate number of blocks*/
        blocks_num = (fw_data->size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
        ui->peStatus->appendPlainText(tr("Total blocks: %1 by %2 bytes").arg(blocks_num).arg(BLOCK_SIZE));
        if (blocks_num > UINT16_MAX)
        {
            ui->peStatus->appendPlainText("Too many blocks. Abort");
            return;
        }

        /*Start to send data*/
        block_id = 0;
        file_offset = 0;
        TimeoutElapsed();

        ui->pbStart->setText("Stop");
    }
    else
    {
        ui->peStatus->appendPlainText("Can't open firmware file");
    }
}

void MainWindow::TimeoutElapsed()
{
    ui->peStatus->appendPlainText(tr("Block sending: %1").arg(block_id));
    BlockWrite(FWTP_MAINSYSTEM_FILE_ID, (uint32_t) fw_data->size(), file_offset, fw_data->mid(file_offset, BLOCK_SIZE));
    sending_timer.start(1000);
}

void MainWindow::ReadUDP()
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
            ui->peStatus->appendPlainText("Invalid CRC");
        }

        struct fwtp_hdr* hdr = (struct fwtp_hdr*) p;

        if (FWTP_HDR_GET_CMD(hdr) == FWTP_CMD_ACK)
        {
            sending_timer.stop();

            ui->pbLoad->setValue((int) (100*(block_id+1)/blocks_num));

            block_id++;
            file_offset += BLOCK_SIZE;

            if (block_id < blocks_num)
            {
                TimeoutElapsed();
            }
            else
            {
                ui->pbStart->setText("Start");
            }
        }
    }
}

uint32_t MainWindow::BlockWrite(uint8_t file_id, uint32_t ttl_size, uint32_t offset, QByteArray data)
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
    return (uint32_t) udp_socket->writeDatagram(packet, QHostAddress(ui->leServer->text()), FWTP_SERVER_PORT);
}
