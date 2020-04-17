#pragma once

#include <QtCore>
#include <QtWebSockets>
#include <QtNetwork>
#include <functional>


namespace Hichess {

enum class Color { White = 0, Black };

struct Packet
{
    enum ContentType : quint8 {
        None = 0,
        UserInfo,
        Message,
        ServerMessage,
        Move,
        Error
    };

    QList<quint8> uint8Buffer = {None};
    ContentType contentType = None;
    QString payload = QString();

    Packet() = default;
    Packet(const QList<quint8>&, const QString&);
    QByteArray serialize();
    static Packet deserialize(const QByteArray&);
    ~Packet() = default;
};

using Username = QString;
using Player = QPair<Username, QWebSocket*>;
using Game = QPair<Player, Player>;
using ProcessPayloadFn_t = std::function<void(QWebSocket*, const QString&)>;

class Server : public QObject
{
    Q_OBJECT

public:
    explicit Server(QObject *parent = nullptr);

private:
    QUdpSocket *m_udpServer;
    QWebSocketServer *m_webServer;
    QQueue<Player> m_playerQueue;
    QMap<Username, QWebSocket*> m_playerMap;
    QSet<Game> m_gameSet;
    QMap<Packet::ContentType, ProcessPayloadFn_t> m_functionMapper;

    void showServerInfo();

    qint64 sendPacket(QWebSocket*, const QList<quint8>&, const QString&);
    void addClient();
    void removeClient(QWebSocket*);

    void processUserInfo(QWebSocket*, const QString&);
    void processMessage(QWebSocket*, const QString&);
    void processMove(QWebSocket*, const QString&);
    void processBinaryMessage(QWebSocket*, const QByteArray&);
};
}
