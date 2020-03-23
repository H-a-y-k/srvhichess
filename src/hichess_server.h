#pragma once

#include <QtCore>
#include <QtWebSockets>
#include <QtNetwork>

typedef QString Username;
typedef QPair<Username, QWebSocket*> Player;
typedef QPair<Player, Player> Game;

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
    QMap<Username, QWebSocket*> m_players;
    QList<Game> m_games;

    void addClient(QWebSocket*);
    void removeClient(QWebSocket*);
};
