#pragma once

#include <QtCore>
#include <QtWebSockets>
#include <QtNetwork>
#include <functional>


namespace Hichess {

struct Packet
{
    enum DetailsType {
        NONE = 0,
        USER_INFO,
        MESSAGE,
        MOVE
    };

    DetailsType detailsType = NONE;
    QString payload;

    Packet() = default;
    Packet(DetailsType detailsType, const QString &payload);
    QByteArray serialize();
    static Packet deserialize(const QByteArray&);
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

private slots:
    void addClient();
    void removeClient(QWebSocket*);

private:
    QUdpSocket *m_udpServer;
    QWebSocketServer *m_webServer;
    QQueue<Player> m_playerQueue;
    QMap<Username, QWebSocket*> m_playerMap;
    QSet<Game> m_gameSet;
    QMap<Packet::DetailsType, ProcessPayloadFn_t> m_functionMapper;

    void showServerInfo();
    void processUserInfo(QWebSocket*, const QString&);
    void processMessage(QWebSocket*, const QString&);
    void processMove(QWebSocket*, const QString&);
    void processBinaryMessage(QWebSocket*, const QByteArray&);
};
}
