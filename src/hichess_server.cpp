#include "hichess_server.h"
#include <QThread>
#include <algorithm>

#define UDP_SERVER_PORT 45454
#define UDP_CLIENT_PORT 45455
#define WEB_PORT 54545


Player::Player(const QString& name, QWebSocket *wsocket)
    : m_wsocket(wsocket)
    , m_name(name)
{}

bool Player::operator==(const Player &other) const
{
    return m_name == other.getName();
}

Game::Game(const PlayerPair &players)
    : m_name(QString("%1 vs %2").arg(players.first.getName(), players.second.getName()))
    , m_players(players)
{}

bool Game::operator==(const Game &other) const
{
    return m_name == other.getName();
}

HichessServer::HichessServer(QObject *parent)
    : QObject(parent)
    , m_udpServer(new QUdpSocket(this))
    , m_webServer(new QWebSocketServer("HichessServer", QWebSocketServer::NonSecureMode, this))
{
    connect(m_udpServer, &QUdpSocket::readyRead, this, &HichessServer::processPendingDatagrams);
    connect(m_webServer, &QWebSocketServer::newConnection, this, &HichessServer::onNewConnection);

    // Find all active interfaces and broadcast
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
                    qDebug() << Q_FUNC_INFO << address.ip().toString();
                    qDebug() << Q_FUNC_INFO << m_udpServer->bind(address.ip(), UDP_SERVER_PORT, QUdpSocket::ShareAddress);
                    qDebug() << Q_FUNC_INFO << m_udpServer->localAddress() << m_udpServer->localPort();
                    qDebug() << Q_FUNC_INFO << m_webServer->listen(address.ip(), WEB_PORT);
                    return;
                }
        }
}

void HichessServer::processPendingDatagrams()
{
    QUdpSocket *socket = qobject_cast<QUdpSocket*>(sender());
    if (socket && socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = socket->receiveDatagram();
        qDebug() << datagram.data();
        if (datagram.data().startsWith("User = ")) {
            QString username = datagram.data().mid(7);
            QRegularExpression rx("[A-Za-z0-9_]{6,15}");

            if (rx.match(username).hasMatch()) {
                auto found = std::find_if(m_allPlayers.begin(), m_allPlayers.end(),
                                          [username](Player p) { return p.getName() == username; });

                if (found == m_allPlayers.end()) {
                    qDebug() << m_webServer->serverUrl().toString().toUtf8();
                    m_usernameQueue.enqueue(username);
                    socket->writeDatagram(m_webServer->serverUrl().toString().toUtf8(), datagram.senderAddress(), UDP_CLIENT_PORT);
                } else
                    qDebug() << username << " username is already occupied";
            } else
                qDebug() << username << " is not a valid username";
        }
    }
}

void HichessServer::addClient(QWebSocket *client)
{
    if (client == nullptr) {
        qDebug() << Q_FUNC_INFO << " Client is nullptr";
        return;
    }

    connect(client, &QWebSocket::textMessageReceived, this, [](){});

    if (!m_usernameQueue.isEmpty()) {
        QString username = m_usernameQueue.dequeue();
        m_playerQueue.enqueue({username, client});
        m_allPlayers.append({username, client});

        if (m_playerQueue.size() > 1) {
            Game game({m_playerQueue.dequeue(), m_playerQueue.dequeue()});
            m_games.append(game);
        }
        foreach (auto p, m_allPlayers)
            qDebug() << p.getName();
    }
}

void HichessServer::onNewConnection()
{
    qDebug() << Q_FUNC_INFO;
    auto thread = QThread::create([this](){addClient(m_webServer->nextPendingConnection()); } );
    thread->start();
}

void HichessServer::onTextMessageReceived()
{

}
