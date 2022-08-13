#ifndef FWTPCLIENT_H
#define FWTPCLIENT_H

#include <QUdpSocket>
#include <QTimer>

class FWTPClient : public QObject
{
    Q_OBJECT

public:
    FWTPClient(uint8_t id, QString server, QString file, uint16_t block);
    ~FWTPClient();

    typedef enum {FILE_START = 0, FILE_SENDING, FILE_STOP, FILE_FINISHED} file_state_t;

public slots:
    void Start();

private slots:
    void ReadUDP();
    uint32_t BlockWrite(uint8_t file_id, uint32_t ttl_size, uint32_t offset, QByteArray data);
    uint32_t StartWrite(uint8_t file_id, uint32_t ttl_size);
    uint32_t StopWrite(uint8_t file_id);
    void TimeoutElapsed();

signals:
    void Finished(void);

private:
    QUdpSocket *udp_socket;
    QByteArray *fw_data;
    uint32_t blocks_num;
    uint16_t block_id;
    uint32_t file_offset;
    QTimer sending_timer;
    file_state_t state;
    uint8_t file_id;
    uint16_t block_size;
    QString server_addr_str;
    QString file_name;
};
#endif // FWTPClient_H
