#pragma once

#include <QtCore>
#include <QtWebSockets>
#include <QtNetwork>
#include <functional>


namespace Hichess {

struct Packet
{
    enum ContentType : uint8_t {
        None = 0,
        PlayerData,
        WhitePlayerData,
        BlackPlayerData,
        Message,
        ServerMessage,
        Move,
        Error
    };

    ContentType contentType = None;
    QString payload = QString();

    Packet() = default;
    Packet(ContentType, const QString&);
    QByteArray serialize();
    static Packet deserialize(const QByteArray&);
    ~Packet() = default;
};

using Username = QString;
using Player = QPair<Username, QWebSocket*>;
using Game = QPair<Player, Player>;
using ProcessPacketFn_t = std::function<void(QWebSocket*, const Packet&)>;

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
    QMap<Packet::ContentType, ProcessPacketFn_t> m_functionMapper;

    void showServerInfo();

    qint64 sendPacket(QWebSocket*, Packet::ContentType, const QString&);
    void addClient();
    void removeClient(QWebSocket*);

    void processPlayerData(QWebSocket*, const Packet &packet);
    void processMessage(QWebSocket*, const Packet &packet);
    void processMove(QWebSocket*, const Packet &packet);
    void processBinaryMessage(QWebSocket*, const QByteArray&);
};
}
