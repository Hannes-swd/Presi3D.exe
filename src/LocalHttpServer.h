#pragma once
#include <QObject>
#include <QString>

class QTcpServer;
class QTcpSocket;

// Minimal static-file HTTP server for previewing the exported presentation
// on http://127.0.0.1 instead of file://. Needed because YouTube (and some
// other embeds) refuse to initialize when the parent page has no http(s)
// origin (YouTube "Error 153").
class LocalHttpServer : public QObject {
    Q_OBJECT
public:
    explicit LocalHttpServer(QObject* parent = nullptr);

    // Starts listening (if not already) and serves files from rootDir.
    // Returns the local base URL, e.g. "http://127.0.0.1:53214", or an
    // empty string on failure.
    QString serve(const QString& rootDir);

private slots:
    void onNewConnection();
    void onRequestReady();

private:
    void handleRequest(QTcpSocket* socket, const QByteArray& requestLine);

    QTcpServer* m_server  = nullptr;
    QString     m_rootDir;
};
