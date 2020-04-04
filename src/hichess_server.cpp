#include "hichess_server.h"
#include <memory>

using namespace Hichess;

namespace {
constexpr auto WEB_PORT = 54545;
}

Packet::Packet(DetailsType detailsType, const QString &payload)
    : detailsType(detailsType)
    , payload(payload)
{}

QByteArray Packet::serialize()
{
    QByteArray bytearray;
    QDataStream datastream(&bytearray, QIODevice::WriteOnly);

    datastream << static_cast<quint8>(detailsType);
    datastream << payload;

    return bytearray;
}

Packet Packet::deserialize(const QByteArray &bytearray)
{
    Packet packet;
    QDataStream datastream(bytearray);

    quint8 detailsT;
    datastream >> detailsT;
    packet.detailsType = static_cast<DetailsType>(detailsT);

    datastream >> packet.payload;

    return packet;
}

Server::Server(QObject *parent)
    : QObject(parent)
{
    m_udpServer = new QUdpSocket(this);
    m_webServer = new QWebSocketServer("Server", QWebSocketServer::NonSecureMode, this);

    using namespace std::placeholders;
    m_functionMapper =
    {
        {Packet::USER_INFO, std::bind(&Server::processUserInfo, this, _1, _2)},
        {Packet::MESSAGE, std::bind(&Server::processMessage, this, _1, _2)},
        {Packet::MOVE, std::bind(&Server::processMove, this, _1, _2)}
    };

    connect(m_webServer, &QWebSocketServer::newConnection, this, &Server::addClient);
    connect(m_webServer, &QWebSocketServer::serverError, this,
            [](QWebSocketProtocol::CloseCode closeCode) { qDebug() << closeCode; });

    QHostAddress address("192.168.56.1");
    qDebug() << "Listening" << m_webServer->listen(address, WEB_PORT);
    qDebug() << "Address" << address;
    qDebug() << "Port" << WEB_PORT;
    qDebug() << "URL" << m_webServer->serverUrl();
}

void Server::showServerInfo()
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


void Server::addClient()
{
    qDebug() << Q_FUNC_INFO;

    auto client = *std::make_unique<QWebSocket*>(m_webServer->nextPendingConnection()).get();

    if (client == nullptr) {
        qDebug() << Q_FUNC_INFO << "There are no pending connections";
        return;
    }
    qDebug() << "Client: " << client;

    connect(client, &QWebSocket::disconnected, std::bind(&Server::removeClient, this, client));
    connect(client, &QWebSocket::binaryMessageReceived, [this, client](const QByteArray &message) {
        processBinaryMessage(client, message);
    });
    connect(client, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this,
            [](QAbstractSocket::SocketError error){ qDebug() << error; });
}

void Server::removeClient(QWebSocket *client)
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

void Server::processUserInfo(QWebSocket *client, const QString &payload)
{
    QRegularExpression rx("[A-Za-z0-9_]{6,15}");
    if (rx.match(payload).hasMatch()) {
        qDebug() << "Username: " << payload;

        m_playerQueue.enqueue({payload, client});
        m_playerMap.insert(payload, client);

        if (m_playerQueue.size() > 1) {
            qDebug() << "There are more than 1 queued players. Found pair for a game...";

            Game game = {m_playerQueue.dequeue(), m_playerQueue.dequeue()};
            if (QRandomGenerator::global()->bounded(true))
                m_gameSet << game;
            else
                m_gameSet << qMakePair(game.second, game.first);
        }
    }

    showServerInfo();
}

void Server::processMessage(QWebSocket *client, const QString &payload)
{

}

void Server::processMove(QWebSocket *client, const QString &payload)
{

}

void Server::processBinaryMessage(QWebSocket *client, const QByteArray &message)
{
    Packet packet = Packet::deserialize(message);

    switch (packet.detailsType) {
    case Packet::NONE:
        break;
    default:
        m_functionMapper[packet.detailsType](client, message);
    }
}
