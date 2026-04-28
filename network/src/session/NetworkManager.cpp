#include <network/session/NetworkManager.h>
#include <network/protocol/Serializer.h>
#include <QDebug>
#include <QNetworkInterface>
#include <QNetworkAddressEntry>

namespace game {
namespace network {

// ═══════════════════════════════════════════════════════════════
// 单例
// ═══════════════════════════════════════════════════════════════
NetworkManager* NetworkManager::instance() {
    static NetworkManager inst;
    return &inst;
}

NetworkManager::NetworkManager(QObject* parent)
    : QObject(parent)
    , m_server(new GameServer(this))
    , m_client(new GameClient(this))
    , m_role(RoomRole::None)
    , m_state(ConnectionState::Disconnected)
{
    connectServerSignals();
    connectClientSignals();
}

NetworkManager::~NetworkManager() {
    disconnect();
}

// ═══════════════════════════════════════════════════════════════
// 创建房间（Host）
// ═══════════════════════════════════════════════════════════════
bool NetworkManager::createRoom(quint16 port) {
    if (m_state != ConnectionState::Disconnected) {
        qWarning() << "[NetworkManager] 已在连接中，无法创建房间";
        return false;
    }

    if (!m_server->startListening(port)) {
        return false;
    }

    setRole(RoomRole::Host);
    qInfo() << "[NetworkManager] 房间创建成功，等待客户端...";
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 加入房间（Client）
// ═══════════════════════════════════════════════════════════════
void NetworkManager::joinRoom(const QString& hostIp, quint16 port) {
    if (m_state != ConnectionState::Disconnected) {
        qWarning() << "[NetworkManager] 已在连接中";
        return;
    }

    setRole(RoomRole::Client);
    m_client->connectToHost(hostIp, port);
}

// ═══════════════════════════════════════════════════════════════
// 断开连接
// ═══════════════════════════════════════════════════════════════
void NetworkManager::disconnect() {
    if (m_role == RoomRole::Host) {
        m_server->disconnect();
    } else if (m_role == RoomRole::Client) {
        m_client->disconnect();
    }

    setRole(RoomRole::None);
    setState(ConnectionState::Disconnected);
    emit disconnected();
}

// ═══════════════════════════════════════════════════════════════
// 发送数据包
// ═══════════════════════════════════════════════════════════════
void NetworkManager::sendPacket(MsgType type, const QByteArray& body) {
    if (m_role == RoomRole::Host && m_server->hasClient()) {
        m_server->sendPacket(type, body);
    } else if (m_role == RoomRole::Client) {
        m_client->sendPacket(type, body);
    } else {
        qWarning() << "[NetworkManager] 无法发送：无连接";
    }
}

// ═══════════════════════════════════════════════════════════════
// 获取本机局域网 IP
// ═══════════════════════════════════════════════════════════════
QString NetworkManager::getLocalIp() const {
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags().testFlag(QNetworkInterface::IsUp) &&
            iface.flags().testFlag(QNetworkInterface::IsRunning) &&
            !iface.flags().testFlag(QNetworkInterface::IsLoopBack))
        {
            for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    return entry.ip().toString();
                }
            }
        }
    }
    return QString("127.0.0.1");
}

// ═══════════════════════════════════════════════════════════════
// 设置状态
// ═══════════════════════════════════════════════════════════════
void NetworkManager::setState(ConnectionState state) {
    if (m_state != state) {
        m_state = state;
        emit stateChanged(state);
    }
}

// ═══════════════════════════════════════════════════════════════
// 设置角色
// ═══════════════════════════════════════════════════════════════
void NetworkManager::setRole(RoomRole role) {
    if (m_role != role) {
        m_role = role;
        emit roleChanged(role);
    }
}

// ═══════════════════════════════════════════════════════════════
// 连接 Server 信号
// ═══════════════════════════════════════════════════════════════
void NetworkManager::connectServerSignals() {
    connect(m_server, &GameServer::clientConnected, this, [this]() {
        setState(ConnectionState::Connected);
        emit connectedToRoom();
    });

    connect(m_server, &GameServer::clientDisconnected, this, [this]() {
        setState(ConnectionState::Disconnected);
        emit disconnected();
    });

    connect(m_server, &GameServer::packetReceived, this, [this](MsgType type, const QByteArray& body) {
        emit packetReceived(type, body);
    });

    connect(m_server, &GameServer::errorOccurred, this, [this](const QString& msg) {
        setState(ConnectionState::Error);
        emit errorOccurred(msg);
    });

    connect(m_server, &GameServer::stateChanged, this, [this](ConnectionState state) {
        setState(state);
    });
}

// ═══════════════════════════════════════════════════════════════
// 连接 Client 信号
// ═══════════════════════════════════════════════════════════════
void NetworkManager::connectClientSignals() {
    connect(m_client, &GameClient::connected, this, [this]() {
        setState(ConnectionState::Connected);
        emit connectedToRoom();
    });

    connect(m_client, &GameClient::disconnected, this, [this]() {
        setState(ConnectionState::Disconnected);
        emit disconnected();
    });

    connect(m_client, &GameClient::packetReceived, this, [this](MsgType type, const QByteArray& body) {
        emit packetReceived(type, body);
    });

    connect(m_client, &GameClient::errorOccurred, this, [this](const QString& msg) {
        setState(ConnectionState::Error);
        emit errorOccurred(msg);
    });

    connect(m_client, &GameClient::stateChanged, this, [this](ConnectionState state) {
        setState(state);
    });
}

} // namespace network
} // namespace game
#include "NetworkManager.moc"
