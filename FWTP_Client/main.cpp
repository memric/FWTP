//#include "mainwindow.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include "fwtp_client.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("FWTP Client");
    QCoreApplication::setApplicationVersion("0.1");

    QCommandLineParser parser;
    parser.setApplicationDescription("FWTP Client");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("server", QCoreApplication::translate("main", "Server address."));
    parser.addPositionalArgument("file", QCoreApplication::translate("main", "Firmware file."));

    // An option with a value
    QCommandLineOption fileID(QStringList() << "i" << "file-id",
                              QCoreApplication::translate("main", "file ID <id>. Default 16."),
                              QCoreApplication::translate("main", "id"),
                              QCoreApplication::translate("main", "16"));
    parser.addOption(fileID);

    QCommandLineOption block_size(QStringList() << "b" << "block-size",
                                  QCoreApplication::translate("main", "Block size <size>. Default 512."),
                                  QCoreApplication::translate("main", "size"),
                                  QCoreApplication::translate("main", "512"));
    parser.addOption(block_size);

    // Process the actual command line arguments given by the user
    parser.process(app);

    /*Parse file ID*/
    uint8_t file_id = 0x10;

    const QString id_str = parser.value(fileID);
    bool ok;
    int temp_id = id_str.toInt(&ok);
    if (ok && temp_id >= 0 && temp_id < 256)
    {
        file_id = (uint8_t) temp_id;
    }
    else
    {
        qDebug() << "Incorrect File id";

        return 1;
    }

    /*Parse Block size*/
    uint16_t block = 512;
    const QString block_str = parser.value(block_size);
    int temp_block = block_str.toInt(&ok);
    if (ok && temp_block >= 0 && temp_block < 1024)
    {
        block = (uint16_t) temp_block;
    }
    else
    {
        qDebug() << "Incorrect Block Size";

        return 1;
    }

    const QStringList args = parser.positionalArguments();

    if (args.isEmpty() || args.length() < 2)
    {
        qDebug() << "Error: Server address and filename must be specified. Type -h for help.";

        return 2;
    }
    else
    {
        FWTPClient *client = new FWTPClient(file_id, args.at(0), args.at(1), block);

        QObject::connect(client, SIGNAL(Finished()), &app, SLOT(quit()));

        client->Start();

        return app.exec();
    }
}
