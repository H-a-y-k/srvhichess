#pragma once

#include <QtCore>
#include <QtWebSockets>
#include <QtNetwork>

using Username = QString;
using Player = QPair<Username, QWebSocket*>;
using Game = QPair<Player, Player>;

class HichessServer : public QObject
{
    Q_OBJECT

public:
    explicit HichessServer(QObject *parent = nullptr);

private slots:
    void processPendingDatagrams();
    void onNewConnection();
    void onTextMessageReceived();

private:
    QUdpSocket *m_udpServer;
    QWebSocketServer *m_webServer;
    QQueue<Username> m_usernameQueue;
    QQueue<Player> m_playerQueue;
    QMap<Username, QWebSocket*> m_playersMap;
    QSet<Game> m_gamesSet;

    void addClient(QWebSocket*);
    void removeClient(QWebSocket*);
};
