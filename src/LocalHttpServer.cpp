#include "LocalHttpServer.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUrl>

LocalHttpServer::LocalHttpServer(QObject* parent) : QObject(parent) {
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &LocalHttpServer::onNewConnection);
}

QString LocalHttpServer::serve(const QString& rootDir) {
    m_rootDir = QDir(rootDir).absolutePath();

    if (!m_server->isListening()) {
        if (!m_server->listen(QHostAddress::LocalHost, 0))
            return {};
    }
    return QString("http://127.0.0.1:%1").arg(m_server->serverPort());
}

void LocalHttpServer::onNewConnection() {
    while (m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, &LocalHttpServer::onRequestReady);
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }
}

void LocalHttpServer::onRequestReady() {
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QByteArray data = socket->readAll();
    int lineEnd = data.indexOf("\r\n");
    QByteArray requestLine = lineEnd >= 0 ? data.left(lineEnd) : data;
    handleRequest(socket, requestLine);
}

static QByteArray mimeTypeFor(const QString& path) {
    QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "html" || ext == "htm") return "text/html; charset=utf-8";
    if (ext == "css")                  return "text/css; charset=utf-8";
    if (ext == "js" || ext == "mjs")   return "application/javascript; charset=utf-8";
    if (ext == "json")                 return "application/json; charset=utf-8";
    if (ext == "png")                  return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif")                  return "image/gif";
    if (ext == "svg")                  return "image/svg+xml";
    if (ext == "webp")                 return "image/webp";
    if (ext == "ico")                  return "image/x-icon";
    if (ext == "woff")                 return "font/woff";
    if (ext == "woff2")                return "font/woff2";
    if (ext == "mp4")                  return "video/mp4";
    if (ext == "webm")                 return "video/webm";
    if (ext == "mp3")                  return "audio/mpeg";
    return "application/octet-stream";
}

void LocalHttpServer::handleRequest(QTcpSocket* socket, const QByteArray& requestLine) {
    // Expect: "GET /some/path HTTP/1.1"
    QList<QByteArray> parts = requestLine.split(' ');
    QByteArray rawPath = parts.size() >= 2 ? parts[1] : QByteArray("/");

    QString path = QUrl(QString::fromUtf8(rawPath)).path();
    if (path.isEmpty() || path == "/") path = "/index.html";

    // Resolve against the export root and reject any path-traversal attempt.
    QString requested = QDir(m_rootDir).filePath(path.mid(1));
    QFileInfo fi(requested);
    QString canonicalRoot = QDir(m_rootDir).canonicalPath();
    if (!canonicalRoot.endsWith('/')) canonicalRoot += '/';
    QString canonicalFile = fi.exists() ? fi.canonicalFilePath() : QString();

    QByteArray body;
    QByteArray status;
    QByteArray contentType = "text/plain; charset=utf-8";

    if (!canonicalFile.isEmpty() && canonicalFile.startsWith(canonicalRoot) && fi.isFile()) {
        QFile f(canonicalFile);
        if (f.open(QIODevice::ReadOnly)) {
            body = f.readAll();
            status = "200 OK";
            contentType = mimeTypeFor(canonicalFile);
        }
    }
    if (status.isEmpty()) {
        status = "404 Not Found";
        body = "404 Not Found";
        contentType = "text/plain; charset=utf-8";
    }

    QByteArray response = "HTTP/1.1 " + status + "\r\n"
        "Content-Type: " + contentType + "\r\n"
        "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n\r\n" + body;

    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}
