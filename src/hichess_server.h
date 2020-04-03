#pragma once

#include <QtCore>
#include <QtWebSockets>
#include <QtNetwork>

using Username = QString;
using Player = QPair<Username, QWebSocket*>;
using Game = QPair<Player, Player>;


namespace Server {
enum PacketType {
    NONE = 0,
    USER_INFO,
    MESSAGE,
    MOVE
};

struct Packet
{
    PacketType type = NONE;
    QString payload;

    static Packet fromByteArray(const QByteArray &bytearray);
};

class HichessServer : public QObject
{
    Q_OBJECT

public:
    explicit HichessServer(QObject *parent = nullptr);

private slots:
    void addClient();

private:
    QUdpSocket *m_udpServer;
    QWebSocketServer *m_webServer;
    QQueue<Player> m_playerQueue;
    QMap<Username, QWebSocket*> m_playerMap;
    QSet<Game> m_gameSet;

    void showServerInfo();
    void removeClient(QWebSocket*);
    void processBinaryMessage(QWebSocket *client, const QByteArray&);
};
}
