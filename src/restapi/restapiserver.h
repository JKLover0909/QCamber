#ifndef RESTAPISERVER_H
#define RESTAPISERVER_H

#include <QObject>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>

class RestApiServer : public QObject
{
    Q_OBJECT

public:
    explicit RestApiServer(quint16 port, QObject *parent = nullptr);
    ~RestApiServer();
    
    bool isListening() const;
    quint16 serverPort() const;

signals:
    void captureRequest(const QJsonObject &request);
    void clientConnected(const QString &clientInfo);
    void clientDisconnected(const QString &clientInfo);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

public slots:
    void sendCaptureResponse(const QString &requestId, const QByteArray &imageData, const QJsonObject &metadata);

private:
    void handleHttpRequest(QTcpSocket *socket, const QByteArray &requestData);
    void sendHttpResponse(QTcpSocket *socket, int statusCode, 
                         const QString &contentType, const QByteArray &body);
    void sendJsonResponse(QTcpSocket *socket, const QJsonObject &json);
    void sendImageResponse(QTcpSocket *socket, const QByteArray &imageData);
    
    QJsonObject parseJsonBody(const QByteArray &body);

    QString generateRequestId();
    void cleanupClient(const QString &requestId);

    QTcpServer *m_server;
    QHash<QTcpSocket*, QByteArray> m_pendingRequests;
    QHash<QString, QTcpSocket*> m_requestClients;
};

#endif // RESTAPISERVER_H