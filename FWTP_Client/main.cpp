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
                                             QCoreApplication::translate("main", "file ID <id>."),
                                             QCoreApplication::translate("main", "id"));
    parser.addOption(fileID);

    // Process the actual command line arguments given by the user
    parser.process(app);

    const QStringList args = parser.positionalArguments();

    if (args.isEmpty() || args.length() < 2)
    {
        qDebug() << "Error: Server address and filename must be specified. Type -h for help.";
    }

    FWTPClient *client = new FWTPClient(args.at(0), args.at(1));

    QObject::connect(client, SIGNAL(Finished()), &app, SLOT(quit()));

    client->Start();

    return app.exec();
}
