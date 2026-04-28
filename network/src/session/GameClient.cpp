#include <network/session/GameClient.h>
#include <network/protocol/Serializer.h>
#include <QDebug>
#include <QDateTime>

namespace game {
namespace network {

constexpr int PING_INTERVAL_MS = 2000;      // 心跳间隔 2 秒
constexpr int PONG_TIMEOUT_MS = 6000;      // 6 秒没收到响应视为断线
constexpr int CONNECT_TIMEOUT_MS = 10000; // 连接超时 10 秒

GameClient::GameClient(QObject* parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_state(ConnectionState::Disconnected)
    , m_serverPort(0)
    , m_pingTimer(new QTimer(this))
    , m_timeoutTimer(new QTimer(this))
    , m_lastPongTime(0)
{
    // 信号连接
    connect(m_socket, &QTcpSocket::connected, this, &GameClient::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &GameClient::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &GameClient::onDisconnected);
    connect(m_socket, &QAbstractSocket::errorOccurred,
            this, &GameClient::onSocketError);

    // 心跳定时器
    m_pingTimer->setInterval(PING_INTERVAL_MS);
    connect(m_pingTimer, &QTimer::timeout, this, &GameClient::onPingTimeout);

    // 连接超时定时器
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
        qWarning() << "[GameClient] 连接超时";
        reset();
        emit errorOccurred("连接超时");
    });
}

GameClient::~GameClient() {
    disconnect();
}

// ═══════════════════════════════════════════════════════════════
// 连接服务器
// ═══════════════════════════════════════════════════════════════
void GameClient::connectToHost(const QString& ip, quint16 port) {
    if (m_state != ConnectionState::Disconnected) {
        qWarning() << "[GameClient] 已在连接中";
        return;
    }

    m_serverIp = ip;
    m_serverPort = port;
    m_state = ConnectionState::Connecting;
    emit stateChanged(m_state);

    qInfo() << "[GameClient] 连接到" << ip << ":" << port;
    m_socket->connectToHost(ip, port);

    // 启动超时计时器
    m_timeoutTimer->start(CONNECT_TIMEOUT_MS);
}

// ═══════════════════════════════════════════════════════════════
// 断开连接
// ═══════════════════════════════════════════════════════════════
void GameClient::disconnect() {
    m_pingTimer->stop();
    m_timeoutTimer->stop();

    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        sendPacket(MsgType::DISCONNECT);
        m_socket->disconnectFromHost();
    }

    reset();
}

// ═══════════════════════════════════════════════════════════════
// 发送数据包
// ═══════════════════════════════════════════════════════════════
void GameClient::sendPacket(MsgType type, const QByteArray& body) {
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "[GameClient] 未连接，无法发送" << msgTypeName(type);
        return;
    }

    QByteArray packet = Serializer::buildPacket(type, body);
    qint64 written = m_socket->write(packet);

    if (written != packet.size()) {
        qWarning() << "[GameClient] 发送不完整:" << written << "/" << packet.size();
    } else {
        qDebug() << "[GameClient] 发送" << msgTypeName(type) << "bodySize=" << body.size();
    }
}

// ═══════════════════════════════════════════════════════════════
// 连接成功
// ═══════════════════════════════════════════════════════════════
void GameClient::onConnected() {
    qInfo() << "[GameClient] 连接成功";

    m_timeoutTimer->stop();
    m_state = ConnectionState::Connected;
    emit stateChanged(m_state);
    emit connected();

    // 启动心跳
    m_pingTimer->start();
    m_lastPongTime = QDateTime::currentMSecsSinceEpoch();
}

// ═══════════════════════════════════════════════════════════════
// 收到数据
// ═══════════════════════════════════════════════════════════════
void GameClient::onReadyRead() {
    m_buffer.append(m_socket->readAll());
    tryParsePackets();
}

// ═══════════════════════════════════════════════════════════════
// 断开连接
// ═══════════════════════════════════════════════════════════════
void GameClient::onDisconnected() {
    qInfo() << "[GameClient] 断开连接";

    m_pingTimer->stop();
    m_timeoutTimer->stop();

    emit disconnected();
    reset();
}

// ═══════════════════════════════════════════════════════════════
// Socket 错误
// ═══════════════════════════════════════════════════════════════
void GameClient::onSocketError(QAbstractSocket::SocketError error) {
    QString msg = m_socket->errorString();
    qWarning() << "[GameClient] Socket错误:" << error << msg;
    emit errorOccurred(msg);
}

// ═══════════════════════════════════════════════════════════════
// 心跳超时检测
// ═══════════════════════════════════════════════════════════════
void GameClient::onPingTimeout() {
    quint64 now = QDateTime::currentMSecsSinceEpoch();

    // 发送 PING
    sendPacket(MsgType::PING);

    // 检查是否超时
    if (now - m_lastPongTime > PONG_TIMEOUT_MS) {
        qWarning() << "[GameClient] 心跳超时，强制断开";
        emit errorOccurred("心跳超时");
        m_socket->disconnectFromHost();
    }
}

// ═══════════════════════════════════════════════════════════════
// 粘包处理（核心！）
// ═══════════════════════════════════════════════════════════════
void GameClient::tryParsePackets() {
    while (true) {
        // 1. 检查包头是否完整
        if (m_buffer.size() < PACKET_HEADER_SIZE) {
            break;
        }

        // 2. 读取包头
        PacketHeader header;
        memcpy(&header, m_buffer.constData(), PACKET_HEADER_SIZE);
        header.bodyLen = qFromBigEndian(header.bodyLen);

        // 3. 检查包体是否完整
        int totalLen = PACKET_HEADER_SIZE + header.bodyLen;
        if (m_buffer.size() < totalLen) {
            break;
        }

        // 4. 提取包体
        QByteArray body = m_buffer.mid(PACKET_HEADER_SIZE, header.bodyLen);
        m_buffer.remove(0, totalLen);

        // 5. 处理特殊消息
        MsgType type = static_cast<MsgType>(header.msgType);

        if (type == MsgType::PING) {
            // 收到 PING，回复 PONG（用 PING 作为心跳响应）
            m_lastPongTime = QDateTime::currentMSecsSinceEpoch();
            qDebug() << "[GameClient] 收到心跳";
            continue;
        }

        if (type == MsgType::DISCONNECT) {
            qInfo() << "[GameClient] 收到断开通知";
            m_socket->disconnectFromHost();
            break;
        }

        qDebug() << "[GameClient] 收到" << msgTypeName(type) << "bodySize=" << body.size();
        emit packetReceived(type, body);
    }
}

// ═══════════════════════════════════════════════════════════════
// 重置状态
// ═══════════════════════════════════════════════════════════════
void GameClient::reset() {
    m_socket->close();
    m_buffer.clear();
    m_state = ConnectionState::Disconnected;
    emit stateChanged(m_state);
}

} // namespace network
} // namespace game
#include "GameClient.moc"
