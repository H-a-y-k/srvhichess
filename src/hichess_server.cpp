#include "hichess_server.h"
#include <memory>

using namespace Server;

namespace {
constexpr auto WEB_PORT = 54545;
}

Packet Packet::fromByteArray(const QByteArray &bytearray)
{
    Packet packet;
    QDataStream datastream(bytearray);

    quint8 type;
    datastream >> type;
    packet.type = static_cast<PacketType>(type);

    datastream >> packet.payload;

    return packet;
}

HichessServer::HichessServer(QObject *parent)
    : QObject(parent)
{
    m_udpServer = new QUdpSocket(this);
    m_webServer = new QWebSocketServer("HichessServer", QWebSocketServer::NonSecureMode, this);

    connect(m_webServer, &QWebSocketServer::newConnection, this, &HichessServer::addClient);
    connect(m_webServer, &QWebSocketServer::serverError, this,
            [](QWebSocketProtocol::CloseCode closeCode) { qDebug() << closeCode; });

    QHostAddress address("192.168.56.1");
    qDebug() << "Listening" << m_webServer->listen(address, WEB_PORT);
    qDebug() << "Address" << address;
    qDebug() << "Port" << WEB_PORT;
    qDebug() << "URL" << m_webServer->serverUrl();
}

void HichessServer::showServerInfo()
{
    QDebug dbg = qDebug().nospace().noquote();

    dbg << "\n===Player queue===\n";
    for (const Player &p : m_playerQueue)
        dbg << "(" << p.first << ", " << p.second << ")\n";
    if (m_playerQueue.isEmpty())
        dbg << "None\n";

    dbg << "\n===Players===\n";
    for (auto it = m_playerMap.begin(); it != m_playerMap.end(); ++it)
        dbg << "(" << it.key() << ", " << it.value() << ")\n";
    if (m_playerMap.isEmpty())
        dbg << "None\n";

    dbg << "\n===Games===\n";
    for (const Game &g : m_gameSet)
        dbg << "(" << g.first.first << " vs " << g.second.first << ")\n";
    if (m_gameSet.isEmpty())
        dbg << "None\n";
}


void HichessServer::addClient()
{
    qDebug() << Q_FUNC_INFO;

    auto client = *std::make_unique<QWebSocket*>(m_webServer->nextPendingConnection()).get();

    if (client == nullptr) {
        qDebug() << Q_FUNC_INFO << "There are no pending connections";
        return;
    }
    qDebug() << "Client: " << client;

    connect(client, &QWebSocket::disconnected, [this, client]() {
        removeClient(client);
    });
    connect(client, &QWebSocket::binaryMessageReceived, [this, client](const QByteArray &message) {
       processBinaryMessage(client, message);
    });
    connect(client, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this,
        [](QAbstractSocket::SocketError error){ qDebug() << error; });
}

void HichessServer::removeClient(QWebSocket *client)
{
    qDebug() << Q_FUNC_INFO;
    if (m_playerMap.key(client) == Username()) {
        qDebug() << " found";
        return;
    }
    qDebug() << m_playerMap;

    Username username = m_playerMap.key(client);
    Player player = {username, client};

    qDebug() << "Found the player to remove" << player;

    m_playerQueue.removeAll(player);
    m_playerMap.remove(username);
    for (auto it = m_gameSet.begin(); it != m_gameSet.end(); ++it)
        if (it->first == player) {
            m_gameSet.erase(it);
            Player secondP = it->second;
            // TODO adjust player removal
            secondP.second->sendTextMessage(QStringLiteral("Player %0 left the game").arg(secondP.first));
            break;
        }

    client->deleteLater();
    qDebug() << "Player" << player << "left the game";

    showServerInfo();
}

void HichessServer::processBinaryMessage(QWebSocket *client, const QByteArray &message)
{
    Packet packet = Packet::fromByteArray(message);

    switch (packet.type) {
    case NONE: {
        break;
    }
    case USER_INFO: {

        Username username = packet.payload;
        qDebug() << "Username: " << username;

        m_playerQueue.enqueue({username, client});
        m_playerMap.insert(username, client);

        if (m_playerQueue.size() > 1) {
            qDebug() << "There are more than 1 queued players. Found pair for a game...";

            Game game = {m_playerQueue.dequeue(), m_playerQueue.dequeue()};
            if (QRandomGenerator::global()->bounded(true))
                m_gameSet << game;
            else
                m_gameSet << qMakePair(game.second, game.first);
        }

        showServerInfo();
        break;
    }
    case MESSAGE: {
        break;
    }
    case MOVE: {
    }
    }
}
