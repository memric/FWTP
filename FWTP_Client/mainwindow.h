#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QUdpSocket>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    typedef enum {FILE_START = 0, FILE_SENDING, FILE_STOP, FILE_FINISHED} file_state_t;

private slots:
    void on_pbFile_clicked();

    void on_pbStart_clicked();

    void ReadUDP();

    uint32_t BlockWrite(uint8_t file_id, uint32_t ttl_size, uint32_t offset, QByteArray data);
    uint32_t StartWrite(uint8_t file_id, uint32_t ttl_size);
    uint32_t StopWrite(uint8_t file_id);
    void TimeoutElapsed();

private:
    Ui::MainWindow *ui;
    QUdpSocket *udp_socket;
    QByteArray *fw_data;
    uint32_t blocks_num;
    uint16_t block_id;
    uint32_t file_offset;
    QTimer sending_timer;
    file_state_t state;
};
#endif // MAINWINDOW_H
