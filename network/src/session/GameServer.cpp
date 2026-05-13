#include <network/session/GameServer.h>
#include <network/protocol/Serializer.h>
#include <QDebug>
#include <QDateTime>
#include <QNetworkInterface>

namespace game {
namespace network {

constexpr int PING_INTERVAL_MS = 2000;      // 心跳间隔 2 秒
constexpr int PONG_TIMEOUT_MS = 6000;       // 6 秒没收到 Pong 视为断线

GameServer::GameServer(QObject* parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_clientSocket(nullptr)
    , m_state(ConnectionState::Disconnected)
    , m_pingTimer(new QTimer(this))
    , m_timeoutTimer(new QTimer(this))
    , m_lastPongTime(0)
{
    // 信号连接
    connect(m_server, &QTcpServer::newConnection, this, &GameServer::onNewConnection);

    // 心跳定时器
    m_pingTimer->setInterval(PING_INTERVAL_MS);
    connect(m_pingTimer, &QTimer::timeout, this, &GameServer::onPingTimeout);
}

GameServer::~GameServer() {
    disconnect();
    reset();
}

// ═══════════════════════════════════════════════════════════════
// 启动监听
// ═══════════════════════════════════════════════════════════════
bool GameServer::startListening(quint16 port) {
    if (m_state != ConnectionState::Disconnected) {
        qWarning() << "[GameServer] 已在监听中，无法重复启动";
        return false;
    }

    if (!m_server->listen(QHostAddress::Any, port)) {
        QString error = m_server->errorString();
        qCritical() << "[GameServer] 监听失败:" << error;
        emit errorOccurred(error);
        return false;
    }

    m_state = ConnectionState::Connecting;
    emit stateChanged(m_state);

    qInfo() << "[GameServer] 开始监听端口" << port;
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 发送数据包
// ═══════════════════════════════════════════════════════════════
void GameServer::sendPacket(MsgType type, const QByteArray& body) {
    if (!m_clientSocket || m_clientSocket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "[GameServer] 客户端未连接，无法发送" << msgTypeName(type);
        return;
    }

    QByteArray packet = Serializer::buildPacket(type, body);
    qint64 written = m_clientSocket->write(packet);

    if (written != packet.size()) {
        qWarning() << "[GameServer] 发送不完整:" << written << "/" << packet.size();
    } else {
        qDebug() << "[GameServer] 发送" << msgTypeName(type) << "bodySize=" << body.size();
    }
}

// ═══════════════════════════════════════════════════════════════
// 主动断开
// ═══════════════════════════════════════════════════════════════
void GameServer::disconnect() {
    m_pingTimer->stop();
    m_timeoutTimer->stop();

    if (m_clientSocket) {
        sendPacket(MsgType::DISCONNECT);
        m_clientSocket->disconnectFromHost();
    }

    reset();
}

// ═══════════════════════════════════════════════════════════════
// 新连接
// ═══════════════════════════════════════════════════════════════
void GameServer::onNewConnection() {
    if (m_clientSocket) {
        // 已有一个客户端，拒绝新连接
        QTcpSocket* reject = m_server->nextPendingConnection();
        reject->disconnectFromHost();
        qWarning() << "[GameServer] 拒绝额外连接（只支持1v1）";
        return;
    }

    m_clientSocket = m_server->nextPendingConnection();

    connect(m_clientSocket, &QTcpSocket::readyRead, this, &GameServer::onReadyRead);
    connect(m_clientSocket, &QTcpSocket::disconnected, this, &GameServer::onClientDisconnected);
    connect(m_clientSocket, &QAbstractSocket::errorOccurred,
            this, &GameServer::onSocketError);

    m_state = ConnectionState::Connected;
    emit stateChanged(m_state);
    emit clientConnected();

    qInfo() << "[GameServer] 客户端连接:" << m_clientSocket->peerAddress().toString();

    // 启动心跳
    m_pingTimer->start();
    m_lastPongTime = QDateTime::currentMSecsSinceEpoch();
}

// ═══════════════════════════════════════════════════════════════
// 收到数据
// ═══════════════════════════════════════════════════════════════
void GameServer::onReadyRead() {
    m_buffer.append(m_clientSocket->readAll());
    tryParsePackets();
}

// ═══════════════════════════════════════════════════════════════
// 客户端断开
// ═══════════════════════════════════════════════════════════════
void GameServer::onClientDisconnected() {
    qInfo() << "[GameServer] 客户端断开";

    m_pingTimer->stop();
    m_timeoutTimer->stop();

    emit clientDisconnected();

    m_clientSocket->deleteLater();
    m_clientSocket = nullptr;
    m_buffer.clear();

    m_state = ConnectionState::Disconnected;
    emit stateChanged(m_state);
}

// ═══════════════════════════════════════════════════════════════
// Socket 错误
// ═══════════════════════════════════════════════════════════════
void GameServer::onSocketError(QAbstractSocket::SocketError error) {
    QString msg = m_clientSocket ? m_clientSocket->errorString() : "未知错误";
    qWarning() << "[GameServer] Socket错误:" << error << msg;
    emit errorOccurred(msg);
}

// ═══════════════════════════════════════════════════════════════
// 心跳超时检测
// ═══════════════════════════════════════════════════════════════
void GameServer::onPingTimeout() {
    quint64 now = QDateTime::currentMSecsSinceEpoch();

    // 发送 PING
    sendPacket(MsgType::PING);

    // 检查是否超时
    if (now - m_lastPongTime > PONG_TIMEOUT_MS) {
        qWarning() << "[GameServer] 心跳超时，强制断开";
        emit errorOccurred("心跳超时");
        m_clientSocket->disconnectFromHost();
    }
}

// ═══════════════════════════════════════════════════════════════
// 粘包处理（核心！）
// ═══════════════════════════════════════════════════════════════
void GameServer::tryParsePackets() {
    while (true) {
        // 1. 检查包头是否完整
        if (m_buffer.size() < PACKET_HEADER_SIZE) {
            break;  // 等待更多数据
        }

        // 2. 读取包头
        PacketHeader header;
        memcpy(&header, m_buffer.constData(), PACKET_HEADER_SIZE);
        header.bodyLen = qFromBigEndian(header.bodyLen);

        // 3. 检查包体是否完整
        int totalLen = PACKET_HEADER_SIZE + header.bodyLen;
        if (m_buffer.size() < totalLen) {
            break;  // 等待更多数据
        }

        // 4. 提取包体
        QByteArray body = m_buffer.mid(PACKET_HEADER_SIZE, header.bodyLen);
        m_buffer.remove(0, totalLen);

        // 5. 处理特殊消息
        MsgType type = static_cast<MsgType>(header.msgType);

        if (type == MsgType::PING) {
            // 收到 PING，立即回 PONG
            sendPacket(MsgType::PONG);
            m_lastPongTime = QDateTime::currentMSecsSinceEpoch();
            qDebug() << "[GameServer] 收到PING，回PONG";
            continue;
        }

        if (type == MsgType::PONG) {
            // 收到 PONG，更新时间戳
            m_lastPongTime = QDateTime::currentMSecsSinceEpoch();
            qDebug() << "[GameServer] 收到PONG ✓";
            continue;
        }

        if (type == MsgType::DISCONNECT) {
            qInfo() << "[GameServer] 收到断开通知";
            m_clientSocket->disconnectFromHost();
            break;
        }

        qDebug() << "[GameServer] 收到" << msgTypeName(type) << "bodySize=" << body.size();
        emit packetReceived(type, body);
    }
}

// ═══════════════════════════════════════════════════════════════
// 重置状态
// ═══════════════════════════════════════════════════════════════
void GameServer::reset() {
    if (m_clientSocket) {
        m_clientSocket->deleteLater();
        m_clientSocket = nullptr;
    }
    m_buffer.clear();
    m_server->close();
    m_state = ConnectionState::Disconnected;
    emit stateChanged(m_state);
}

} // namespace network
} // namespace game
#include "GameServer.moc"
