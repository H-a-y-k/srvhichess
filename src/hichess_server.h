#pragma once

#include <QtCore>
#include <QtWebSockets>
#include <QtNetwork>

class Player
{
public:
    Player(const QString&, QWebSocket*);
    bool operator==(const Player&);

    QString getName() const { return m_name; }
    QWebSocket* getWSocket() const { return m_wsocket; }

private:
    QWebSocket *m_wsocket;
    QString m_name;
};

typedef QPair<Player, Player> PlayerPair;

class Game
{
public:
    Game(const PlayerPair&);
    bool operator==(const Game&);

    QString getName() const { return m_name; }
    PlayerPair getPlayers() const { return m_players; }

private:
    QString m_name;
    PlayerPair m_players;
};

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
    QQueue<Player> m_playerQueue;
    QSet<Player> m_allPlayers;
    QSet<Game> m_games;

    void addClient(QWebSocket*);
};
