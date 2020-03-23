#include "hichess_server.h"

#define UDP_SERVER_PORT 45454
#define UDP_CLIENT_PORT 45455
#define WEB_PORT 54545

namespace {
Username getUsername(const Player &p)
{
    return p.first;
}

QWebSocket* getWSocket(const Player &p)
{
    return p.second;
}

QString getGameName(const Game &g)
{
    return getUsername(g.first) + " vs " + getUsername(g.second);
}
}


HichessServer::HichessServer(QObject *parent)
    : QObject(parent)
{
    m_udpServer = new QUdpSocket(this);
    m_webServer = new QWebSocketServer("HichessServer", QWebSocketServer::NonSecureMode, this);

    connect(m_udpServer, &QUdpSocket::readyRead, this, &HichessServer::processPendingDatagrams);
    connect(m_webServer, &QWebSocketServer::newConnection, this, &HichessServer::onNewConnection);
    connect(m_webServer, &QWebSocketServer::serverError, this,
            [](QWebSocketProtocol::CloseCode closeCode) { qDebug() << Q_FUNC_INFO << closeCode; });

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

void HichessServer::processPendingDatagrams()
{
    qDebug() << "Processing pending datagrams...";

    QUdpSocket *socket = qobject_cast<QUdpSocket*>(sender());
    if (socket && socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = socket->receiveDatagram();
        qDebug() << "Datagram data: " << datagram.data();

        if (datagram.data().startsWith("User = ")) {
            Username username = datagram.data().mid(7);
            QRegularExpression rx("[A-Za-z0-9_]{6,15}");

            if (rx.match(username).hasMatch()) {
                qDebug() << "Parsed username is valid...";
                auto found = std::find_if(m_players.begin(), m_players.end(), [username](const Player &p) {
                    return QString::compare(p.first, username, Qt::CaseInsensitive);
                });

                if (found == m_players.end()) {
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
}

void HichessServer::addClient(QWebSocket *client)
{
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

        QString username = m_usernameQueue.dequeue();
        qDebug() << "Username: " << username;

        m_playerQueue.enqueue({username, client});
        m_players.insert(username, client);

        if (m_playerQueue.size() > 1) {
            qDebug() << Q_FUNC_INFO << "There are more than 1 queued players. Found pair for a game...";
            Game game = {m_playerQueue.dequeue(), m_playerQueue.dequeue()};
            m_games.append(game);
        }

        qDebug() << m_players;
    } else
        qDebug() << Q_FUNC_INFO << "There are no queued usernames";

    qDebug() << "";
}

void HichessServer::removeClient(QWebSocket *client)
{
    auto p = std::find_if(m_players.begin(), m_players.end(), [client](const Player &p){ return p.second == client; });
    if (p != m_players.end()){
        qDebug() << Q_FUNC_INFO << "Found the client to remove";
        qDebug() << Q_FUNC_INFO << "Username: " << p.key() << ", socket: " << client;


        std::remove_if(m_games.first(), m_games.last(), [p](const Game &g) {
            return g.first.second == p.value() || g.first.second == p.value();
        });
        p.value()->deleteLater();
        qDebug() << Q_FUNC_INFO << p.key() << " left the game";
    }
}

void HichessServer::onNewConnection()
{
    auto cli = m_webServer->nextPendingConnection();
    addClient(cli);
}

void HichessServer::onTextMessageReceived()
{

}

/*
 *         foreach (Game g, m_games) {
            if (g.getWhite() == *p) {
                qDebug() << "Found game with insufficient players. White player is missing";

                foreach (const auto &_g, m_games)
                    if (_g.getName() == g.getName())
                        m_games.removeAll(_g);

                qDebug() << "Games after removal:";
                foreach (const auto &g, m_games)
                    qDebug() << g.getName();

                g.getBlack().getWSocket()->sendTextMessage(p->getName() + " left the game");
                break;
            } else if (g.getBlack() == *p) {
                qDebug() << Q_FUNC_INFO << "Found game with insufficient players. Black player is missing";

                foreach (const auto &_g, m_games)
                    if (_g.getName() == g.getName())
                        m_games.removeAll(_g);

                qDebug() << "Games after removal:";
                foreach (const auto &g, m_games)
                    qDebug() << g.getName();

                g.getBlack().getWSocket()->sendTextMessage(p->getName() + " left the game");
                break;
            } else
                qDebug() << Q_FUNC_INFO << "Client is not inside a game";
        }
        */
