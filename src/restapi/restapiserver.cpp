#include "restapiserver.h"
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QUuid>
#include <QDateTime>
#include <QTimer>

#ifdef _MSC_VER
#pragma comment(lib, "Qt6Network.lib")
#endif

RestApiServer::RestApiServer(quint16 port, QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    if (m_server->listen(QHostAddress::Any, port)) {
        qDebug() << "REST API Server listening on port" << port;
        connect(m_server, &QTcpServer::newConnection,
                this, &RestApiServer::onNewConnection);
    } else {
        qDebug() << "Failed to start REST API server:" << m_server->errorString();
    }
}

RestApiServer::~RestApiServer()
{
    if (m_server) {
        m_server->close();
    }
}

bool RestApiServer::isListening() const
{
    return m_server && m_server->isListening();
}

quint16 RestApiServer::serverPort() const
{
    return m_server ? m_server->serverPort() : 0;
}

void RestApiServer::onNewConnection()
{
    QTcpSocket *socket = m_server->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &RestApiServer::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &RestApiServer::onDisconnected);
    
    m_pendingRequests[socket] = QByteArray();
    
    emit clientConnected(socket->peerAddress().toString());
}

void RestApiServer::onReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    m_pendingRequests[socket].append(socket->readAll());
    
    // Check if we have a complete HTTP request
    QByteArray& data = m_pendingRequests[socket];
    if (data.contains("\r\n\r\n")) {
        handleHttpRequest(socket, data);
        data.clear();
    }
}

void RestApiServer::onDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    emit clientDisconnected(socket->peerAddress().toString());
    
    // ✅ THÊM: Remove khỏi request tracking
    QStringList keysToRemove;
    for (auto it = m_requestClients.begin(); it != m_requestClients.end(); ++it) {
        if (it.value() == socket) {
            keysToRemove.append(it.key());
        }
    }
    for (const QString &key : keysToRemove) {
        m_requestClients.remove(key);
    }
    
    m_pendingRequests.remove(socket);
    socket->deleteLater();
}

void RestApiServer::handleHttpRequest(QTcpSocket *socket, const QByteArray &requestData)
{
    // Parse HTTP request
    QString request = QString::fromUtf8(requestData);
    QStringList lines = request.split("\r\n");
    
    if (lines.isEmpty()) return;
    
    // Parse request line: "POST /api/capture HTTP/1.1"
    QStringList requestLine = lines[0].split(" ");
    if (requestLine.size() < 3) return;
    
    QString method = requestLine[0];
    QString path = requestLine[1];
    
    // Find body (after empty line)
    int bodyStart = request.indexOf("\r\n\r\n");
    QByteArray body;
    if (bodyStart != -1) {
        body = requestData.mid(bodyStart + 4);
    }
    
    // Route handling
    if (method == "POST" && path == "/api/capture") {
        QJsonObject json = parseJsonBody(body);
        
        // ✅ Generate và thêm requestId
        QString requestId;
        if (json.contains("requestId") && !json["requestId"].toString().isEmpty()) {
            requestId = json["requestId"].toString();
        } else {
            requestId = generateRequestId();
            json["requestId"] = requestId;
        }
        
        // ✅ Track client socket
        m_requestClients[requestId] = socket;
        
        qDebug() << "Capture request received, requestId:" << requestId;
        qDebug() << "Keeping connection open, waiting for capture to complete...";
        
        // ✅ Emit signal để ViewerWindow xử lý
        emit captureRequest(json);
        
        // ✅ KHÔNG GỬI ACKNOWLEDGMENT NGAY!
        // Đợi ViewerWindow gọi sendCaptureResponse() để gửi ảnh
        // Connection sẽ được giữ mở cho đến khi nhận được ảnh
        
        return;  // Exit sớm, giữ connection mở
    }
    else if (method == "GET" && path == "/api/status") {
        QJsonObject response;
        response["status"] = "ok";
        response["port"] = static_cast<int>(m_server->serverPort());
        sendJsonResponse(socket, response);
    }
    else {
        // 404 Not Found
        QJsonObject response;
        response["error"] = "Endpoint not found";
        sendHttpResponse(socket, 404, "application/json", 
                        QJsonDocument(response).toJson());
    }
}

void RestApiServer::sendHttpResponse(QTcpSocket *socket, int statusCode,
                                     const QString &contentType, const QByteArray &body)
{
    QString statusText;
    switch (statusCode) {
        case 200: statusText = "OK"; break;
        case 404: statusText = "Not Found"; break;
        case 500: statusText = "Internal Server Error"; break;
        default: statusText = "Unknown";
    }
    
    QString headers = QString(
        "HTTP/1.1 %1 %2\r\n"
        "Content-Type: %3\r\n"
        "Content-Length: %4\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n"
    ).arg(statusCode).arg(statusText).arg(contentType).arg(body.size());
    
    socket->write(headers.toUtf8());
    socket->write(body);
    socket->flush();
}

void RestApiServer::sendJsonResponse(QTcpSocket *socket, const QJsonObject &json)
{
    QByteArray body = QJsonDocument(json).toJson();
    sendHttpResponse(socket, 200, "application/json", body);
}

void RestApiServer::sendImageResponse(QTcpSocket *socket, const QByteArray &imageData)
{
    sendHttpResponse(socket, 200, "image/png", imageData);
}

QJsonObject RestApiServer::parseJsonBody(const QByteArray &body)
{
    QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isObject()) {
        return doc.object();
    }
    return QJsonObject();
}

void RestApiServer::sendCaptureResponse(const QString &requestId,
                                        const QByteArray &imageData, 
                                        const QJsonObject &metadata)
{
    qDebug() << "=== sendCaptureResponse called ===";
    qDebug() << "Request ID:" << requestId;
    qDebug() << "Image size:" << imageData.size() << "bytes";
    
    // ✅ Tìm client socket
    QTcpSocket *socket = m_requestClients.value(requestId, nullptr);
    
    if (!socket) {
        qDebug() << "ERROR: Client socket not found for request:" << requestId;
        qDebug() << "Available request IDs:" << m_requestClients.keys();
        return;
    }
    
    qDebug() << "Socket found, state:" << socket->state();
    qDebug() << "Socket valid:" << socket->isValid();
    
    if (!socket->isValid() || socket->state() != QTcpSocket::ConnectedState) {
        qDebug() << "ERROR: Socket is not connected, state:" << socket->state();
        cleanupClient(requestId);
        return;
    }
    
    qDebug() << "✅ Sending image response...";
    // ✅ Option 1: Gửi ảnh trực tiếp (đơn giản hơn)
    sendImageResponse(socket, imageData);
    qDebug() << "✅ Image sent successfully!";
    
    // ✅ Option 2: Gửi JSON metadata trước, sau đó gửi ảnh (nếu cần)
    // QJsonObject response = metadata;
    // response["imageSize"] = imageData.size();
    // response["format"] = "PNG";
    // sendJsonResponse(socket, response);
    // socket->write(imageData);
    // socket->flush();
    
    // ✅ Cleanup sau khi gửi xong
    QTimer::singleShot(1000, this, [this, requestId]() {
        cleanupClient(requestId);
    });
}

QString RestApiServer::generateRequestId()
{
    QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return QString("req_%1_%2").arg(timestamp).arg(uuid.left(8));
}

void RestApiServer::cleanupClient(const QString &requestId)
{
    QTcpSocket *socket = m_requestClients.value(requestId, nullptr);
    if (socket) {
        socket->disconnectFromHost();
        m_requestClients.remove(requestId);
    }
}

