#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QRunnable>
#include <QTcpSocket>
#include <QThreadPool>
#include <QTimer>
#include <iostream>

#include "settings.hpp"

QByteArray IntToArray(qint64 source) {
    QByteArray temp;
    QDataStream data(&temp, QIODevice::ReadWrite);
    data.setByteOrder(QDataStream::ByteOrder::LittleEndian);
    data << source;
    return temp;
}

class ProcessFileOnServer : public QRunnable {
   public:
    explicit ProcessFileOnServer(QTextStream& output, QString filePath)
        : QRunnable(), output(output), filePath(std::move(filePath)) {}

    void run() override {
        QTcpSocket tcpSocket;
        tcpSocket.connectToHost(SERVER_IP, SERVER_PORT);
        tcpSocket.waitForConnected();
        if (tcpSocket.state() ==
            QAbstractSocket::SocketState::UnconnectedState) {
            std::cerr << "Can't connect to server"
                      << std::endl;
            return;
        }
        if (!sendFile(tcpSocket)) {
            return;
        }
        output << "Result: " << readResponce(tcpSocket) << "\n";
        output.flush();
        tcpSocket.disconnectFromHost();
        if (tcpSocket.state() !=
            QAbstractSocket::SocketState::UnconnectedState) {
            tcpSocket.waitForDisconnected();
        }
    }

   private:
    QTextStream& output;
    QString filePath;

    bool sendFile(QTcpSocket& tcpSocket) {
        QFile inFile(filePath);
        if (!inFile.open(QIODevice::ReadOnly)) {
            std::cerr << "error: " << inFile.errorString().toStdString()
                      << "\n";
            return false;
        }
        tcpSocket.write(IntToArray(inFile.size()));
        while (true) {
            auto buf = inFile.read(BUFFER_SIZE);
            if (buf.size() == 0) {
                break;
            }
            tcpSocket.write(buf);
            if (!tcpSocket.waitForBytesWritten()) {
                if (tcpSocket.error() !=
                    QAbstractSocket::SocketError::UnknownSocketError) {
                    std::cerr
                        << "error: " << tcpSocket.errorString().toStdString()
                        << "\n";
                }
                return false;
            }
        }
        return true;
    }

    QString readResponce(QTcpSocket& tcpSocket) {
        QString responce;
        while (true) {
            tcpSocket.waitForReadyRead();
            auto receivedBytes = tcpSocket.readAll();
            responce.append(receivedBytes);
            if (receivedBytes.back() == '\0') {
                break;
            }
        }
        return responce;
    }
};

void appLoop() {
    QTextStream output(stdout);
    QTextStream input(stdin);
    while (true) {
        output << "Write path in the following form: \"path/to/file\""
               << " or q to exit\n";
        output.flush();
        auto command = input.readLine();
        command = command.trimmed();
        if (command.size() == 0) {
            continue;
        }
        if (command == "q") {
            return;
        }
        if (command.front() != '\"' || command.back() != '\"' ||
            command.size() < 2) {
            output << "Path should be incapsulated inside \"\", but got: "
                   << command << "\n";
            output.flush();
            continue;
        }
        auto pathStr = command.mid(1, command.size() - 2);
        QFileInfo path(pathStr);
        if (!path.exists()) {
            output << "File at absolute path: \"" << path.absoluteFilePath()
                   << "\" is not found\n";
            output.flush();
            continue;
        }
        output << "Processing will be done in background...\n";
        output.flush();

        // we don't have gui, but anyway to not block "gui" thread
        ProcessFileOnServer* processFile =
            new ProcessFileOnServer(output, path.filePath());
        QThreadPool::globalInstance()->start(processFile);
    }
    output << "Waiting for all unfinished tasks...\n";
    output.flush();
    QThreadPool::globalInstance()->waitForDone();
}

int main(int argc, char* argv[]) {
    QCoreApplication a(argc, argv);
    appLoop();
}
