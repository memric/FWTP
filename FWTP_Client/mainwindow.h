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

private slots:
    void on_pbFile_clicked();

    void on_pbStart_clicked();

    void ReadUDP();

    uint32_t BlockWrite(uint8_t file_id, uint32_t ttl_size, uint32_t offset, QByteArray data);
    void TimeoutElapsed();

private:
    Ui::MainWindow *ui;
    QUdpSocket *udp_socket;
    QByteArray *fw_data;
    uint32_t blocks_num;
    uint16_t block_id;
    uint32_t file_offset;
    QTimer sending_timer;
};
#endif // MAINWINDOW_H
