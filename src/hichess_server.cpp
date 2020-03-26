#include "hichess_server.h"

namespace {
constexpr auto UDP_SERVER_PORT = 45454;
constexpr auto UDP_CLIENT_PORT = 45455;
constexpr auto WEB_PORT = 54545;
}

HichessServer::HichessServer(QObject *parent)
    : QObject(parent)
{
    m_udpServer = new QUdpSocket(this);
    m_webServer = new QWebSocketServer("HichessServer", QWebSocketServer::NonSecureMode, this);

    connect(m_udpServer, &QUdpSocket::readyRead, this, &HichessServer::processPendingDatagrams);
    connect(m_webServer, &QWebSocketServer::newConnection, this, &HichessServer::onNewConnection);
    connect(m_webServer, &QWebSocketServer::serverError, this,
            [](QWebSocketProtocol::CloseCode closeCode) { qDebug() << closeCode; });

    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    foreach (QNetworkInterface interface, interfaces)
        if (interface.isValid() &&
            !interface.flags().testFlag(QNetworkInterface::IsLoopBack) &&
            !interface.flags().testFlag(QNetworkInterface::IsPointToPoint) &&
            interface.flags().testFlag(QNetworkInterface::IsUp) &&
            interface.flags().testFlag(QNetworkInterface::IsRunning))
        {
            QList<QNetworkAddressEntry> addresses = interface.addressEntries();
            foreach (QNetworkAddressEntry address, addresses)
                if (!address.ip().isNull() &&
                    !address.ip().isLoopback() &&
                    address.ip().protocol() == QUdpSocket::IPv4Protocol)
                {
                    qDebug() << "IP:" << address.ip().toString();
                    qDebug() << "Bound:" << m_udpServer->bind(address.ip(), UDP_SERVER_PORT, QUdpSocket::ShareAddress);
                    qDebug() << "Port:" << m_udpServer->localPort();
                    qDebug() << "Listening:" << m_webServer->listen(address.ip(), WEB_PORT) << '\n';
                    return;
                }
        }
}

void HichessServer::showServerInfo()
{
    QDebug dbg = qDebug().nospace().noquote();

    dbg << "\n===Player queue===\n";
    foreach (const Player &p, m_playerQueue)
        dbg << "(" << p.first << ", " << p.second << ")\n";

    dbg << "\n===Players===\n";
    foreach (const Username &name, m_playersMap.keys())
        dbg << "(" << name << ", " << m_playersMap[name] << ")\n";

    dbg << "\n===Games===\n";
    foreach (const Game &g, m_gamesSet)
        dbg << "(" << g.first.first << " - " << g.second.first << ")\n";
}

void HichessServer::processPendingDatagrams()
{
    qDebug() << Q_FUNC_INFO;

    auto *socket = qobject_cast<QUdpSocket*>(sender());
    if (socket && socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = socket->receiveDatagram();
        qDebug() << "Datagram data: " << datagram.data();

            Username username = datagram.data();
            QRegularExpression rx("[A-Za-z0-9_]{6,15}");

        if (rx.match(username).hasMatch()) {
            qDebug() << "Parsed username is valid...";

            if (!m_usernameQueue.contains(username)) {
                qDebug() << username << "username is not occupied";
                qDebug() << "Url:" << m_webServer->serverUrl().toString().toUtf8();
                m_usernameQueue.enqueue(username);
                socket->writeDatagram(m_webServer->serverUrl().toString().toUtf8(), datagram.senderAddress(), UDP_CLIENT_PORT);
            } else
                qDebug() << username << "username is already occupied";
        } else
            qDebug() << username << "is not a valid username";
    }
}

void HichessServer::addClient(QWebSocket *client)
{
    qDebug() << Q_FUNC_INFO;

    if (client == nullptr) {
        qDebug() << Q_FUNC_INFO << "Client is nullptr";
        return;
    }
    qDebug() << "Client: " << client;

    connect(client, &QWebSocket::disconnected, this, [this, client]() {
        qDebug() << client << "disconnected";
        removeClient(client);
    });
    connect(client, &QWebSocket::textMessageReceived, this, [](){});
    connect(client, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
        [=](QAbstractSocket::SocketError error){ qDebug() << Q_FUNC_INFO << error; });

    if (!m_usernameQueue.isEmpty()) {
//        qDebug() << "There are queued usernames";

        Username username = m_usernameQueue.dequeue().toLower();
        qDebug() << "Username: " << username;

        m_playerQueue.enqueue({username, client});
        m_playersMap.insert(username, client);

        if (m_playerQueue.size() > 1) {
            qDebug() << Q_FUNC_INFO << "There are more than 1 queued players. Found pair for a game...";

            Game game = {m_playerQueue.dequeue(), m_playerQueue.dequeue()};
            if (QRandomGenerator::global()->bounded(true))
                m_gamesSet << game;
            else
                m_gamesSet << qMakePair(game.second, game.first);
        }

        showServerInfo();
    } else
        qDebug() << Q_FUNC_INFO << "There are no queued usernames";

    qDebug() << "";
}

void HichessServer::removeClient(QWebSocket *client)
{
    qDebug() << Q_FUNC_INFO;
    if (m_playersMap.key(client) == Username()) {
        qDebug() << "Client not found";
        return;
    }
    qDebug() << m_playersMap;

    Username username = m_playersMap.key(client);
    Player player = {username, client};

    qDebug() << "Found the player to remove" << player;

    m_playerQueue.removeAll(player);
    m_playersMap.remove(username);
    for(auto it = m_gamesSet.begin(); it != m_gamesSet.end(); ++it)
        if (it->first == player || it->second == player) {
            m_gamesSet.erase(it);
            break;
        }

    client->deleteLater();
    qDebug() << player << " left the game";

    showServerInfo();
}

void HichessServer::onNewConnection()
{
    auto cli = m_webServer->nextPendingConnection();
    addClient(cli);
}

void HichessServer::onTextMessageReceived()
{

}
