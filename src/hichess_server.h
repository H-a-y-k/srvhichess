#pragma once

#include <QtCore>
#include <QtWebSockets>
#include <functional>
#include <utility>

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
        Errors
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
using Player = std::pair<Username, QWebSocket*>;
using Game = std::pair<Player, Player>;
using ProcessPacketFn_t = std::function<void(QWebSocket*, const Packet&)>;

class Server : public QObject
{
    Q_OBJECT

public:
    explicit Server(QObject *parent = nullptr);

private:
    QWebSocketServer *m_webServer;
    QQueue<Player> m_playerQueue;
    QMap<Username, QWebSocket*> m_playerMap;
    QSet<Game> m_gameSet;
    QMap<Packet::ContentType, ProcessPacketFn_t> m_functionMapper;

    void showServerInfo();
    std::pair<QWebSocket*, QSet<Game>::iterator> getOpponentClientOf(QWebSocket*);

    qint64 sendPacket(QWebSocket*, Packet::ContentType, const QString&);
    void addClient();
    void removeClient(QWebSocket*);

    void processPlayerData(QWebSocket*, const Packet&);
    void processMessage(QWebSocket*, const Packet&);
    void processMove(QWebSocket*, const Packet&);
    void processBinaryMessage(QWebSocket*, const QByteArray&);
};
}
