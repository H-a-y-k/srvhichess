#include "hichess_server.h"
#include <memory>
#include <random>

using namespace Hichess;

namespace {
constexpr auto WEB_PORT = 54545;

bool isUsernameValid(const Username &username)
{
    QRegularExpression rx("[A-Za-z0-9_]{6,15}");
    return rx.match(username).hasMatch();
}

Game makeRandomShuffledGame(const Player &p1, const Player &p2)
{
    std::default_random_engine engine;
    std::bernoulli_distribution dist(0.5);

    if (dist(engine))
        return std::make_pair(p1, p2);
    return std::make_pair(p2, p1);
}
}

Packet::Packet(ContentType contentType, const QString &payload)
    : contentType(contentType)
    , payload(payload)
{}

QByteArray Packet::serialize()
{
    QByteArray bytearray;
    QDataStream datastream(&bytearray, QIODevice::WriteOnly);

    datastream << static_cast<uint8_t>(contentType);
    datastream << payload;

    return bytearray;
}

Packet Packet::deserialize(const QByteArray &bytearray)
{
    Packet packet;
    QDataStream datastream(bytearray);

    uint8_t contentType;
    datastream >> contentType;
    packet.contentType = static_cast<ContentType>(contentType);

    datastream >> packet.payload;

    return packet;
}

Server::Server(QObject *parent)
    : QObject(parent)
{
    m_webServer = new QWebSocketServer("Server", QWebSocketServer::NonSecureMode, this);

    using namespace std::placeholders;
    m_functionMapper =
    {
        {Packet::PlayerData, std::bind(&Server::processPlayerData, this, _1, _2)},
        {Packet::Message, std::bind(&Server::processMessage, this, _1, _2)},
        {Packet::Move, std::bind(&Server::processMove, this, _1, _2)}
    };

    connect(m_webServer, &QWebSocketServer::newConnection, this, &Server::addClient);
    connect(m_webServer, &QWebSocketServer::serverError, this,
            [](QWebSocketProtocol::CloseCode closeCode) { qDebug() << closeCode; });

    QHostAddress address("192.168.1.2");
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
    for (auto it = m_playerMap.begin(); it != m_playerMap.end(); it++)
        dbg << "(" << it.key() << ", " << it.value() << ")\n";
    if (m_playerMap.isEmpty())
        dbg << "None\n";

    dbg << "\n===Games===\n";
    for (const Game &g : m_gameSet)
        dbg << "(" << g.first.first << " vs " << g.second.first << ")\n";
    if (m_gameSet.isEmpty())
        dbg << "None\n";
}

std::pair<QWebSocket*, QSet<Game>::iterator> Server::getOpponentClientOf(QWebSocket *client1)
{
    QWebSocket *client2 = nullptr;

    for (auto it = m_gameSet.begin(); it != m_gameSet.end(); it++) {
        if (it->first.second == client1)
            client2 = it->second.second;
        else if (it->second.second == client1)
            client2 = it->first.second;

        if (client2 != nullptr)
            return std::make_pair(client2, it);
    }

    return std::make_pair(client2, m_gameSet.end());
}

qint64 Server::sendPacket(QWebSocket *client, Packet::ContentType contentType, const QString &payload)
{
    Packet packet(contentType, payload);
    return client->sendBinaryMessage(packet.serialize());
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

    connect(client, &QWebSocket::disconnected,
            std::bind(&Server::removeClient, this, client));
    connect(client, &QWebSocket::binaryMessageReceived,
            std::bind(&Server::processBinaryMessage, this, client, std::placeholders::_1));
    connect(client, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this,
            [](QAbstractSocket::SocketError error){ qDebug() << error; });
}

void Server::removeClient(QWebSocket *client)
{
    qDebug() << Q_FUNC_INFO;
    if (m_playerMap.key(client).isEmpty()) {
        qDebug() << "Client not found";
        return;
    }
    qDebug() << m_playerMap;

    Username username = m_playerMap.key(client);
    Player player1(username, client);

    m_playerQueue.removeAll(player1);
    m_playerMap.remove(username);

    if (auto [opponentClient, it] = getOpponentClientOf(client); opponentClient != nullptr) {
        m_gameSet.erase(it);
        sendPacket(opponentClient, Packet::Message,
                   QStringLiteral("Player %0 left the game").arg(player1.first));
    }

    client->deleteLater();
    qDebug() << "Player" << player1 << "left the game";

    showServerInfo();
}

void Server::processPlayerData(QWebSocket *client, const Packet &packet)
{
    Username username = packet.payload;

    if (isUsernameValid(username) && m_playerMap.find(username) == m_playerMap.end()) {
        qDebug() << "Username: " << username;

        m_playerMap.insert(username, client);

        if (auto player1 = Player(username, client); !m_playerQueue.isEmpty()) {
            qDebug() << "There are more than 1 queued players. Found pair for a game...";

            Game game = makeRandomShuffledGame(player1, m_playerQueue.dequeue());

            m_gameSet << game;

            sendPacket(game.first.second, Packet::WhitePlayerData, game.second.first);
            sendPacket(game.second.second, Packet::BlackPlayerData, game.first.first);
        } else
            m_playerQueue.enqueue(player1);
    } else
        sendPacket(client, Packet::Errors, "Invalid player data");

    showServerInfo();
}

void Server::processMessage(QWebSocket *client, const Packet &packet)
{
    auto [opponent, _] = getOpponentClientOf(client); Q_UNUSED(_)
    sendPacket(opponent, Packet::Message, packet.payload);
}

void Server::processMove(QWebSocket *client, const Packet &packet)
{
    QString move = packet.payload;
    auto [opponentClient, _] = getOpponentClientOf(client); Q_UNUSED(_)
    sendPacket(opponentClient, Packet::Move, move);
}

void Server::processBinaryMessage(QWebSocket *client, const QByteArray &message)
{
    Packet packet = Packet::deserialize(message);
    qDebug() << packet.contentType << packet.payload;

    if (packet.contentType != Packet::None)
        m_functionMapper[packet.contentType](client, packet);
}
