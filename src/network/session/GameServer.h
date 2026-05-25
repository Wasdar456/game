#ifndef GAME_SERVER_H
#define GAME_SERVER_H

#include "../protocol/ProtocolDef.h"
#include "NetworkState.h"
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QByteArray>

namespace game {
namespace network {

// ═══════════════════════════════════════════════════════════════
// GameServer - Host 端：监听端口，接受客户端连接
// ═══════════════════════════════════════════════════════════════
class GameServer : public QObject {
    Q_OBJECT

public:
    explicit GameServer(QObject* parent = nullptr);
    ~GameServer();

    // ═══════════════════════════════════════════════════════════
    // 启动监听
    // ═══════════════════════════════════════════════════════════
    bool startListening(quint16 port = 9527);

    // ═══════════════════════════════════════════════════════════
    // 发送数据包
    // ═══════════════════════════════════════════════════════════
    void sendPacket(MsgType type, const QByteArray& body = {});

    // ═══════════════════════════════════════════════════════════
    // 主动断开连接
    // ═══════════════════════════════════════════════════════════
    void disconnect();

    // ═══════════════════════════════════════════════════════════
    // 获取状态
    // ═══════════════════════════════════════════════════════════
    ConnectionState state() const { return m_state; }
    bool hasClient() const { return m_clientSocket != nullptr; }

signals:
    // ═══════════════════════════════════════════════════════════
    // 信号定义
    // ═══════════════════════════════════════════════════════════
    void clientConnected();                       // 客户端连接
    void clientDisconnected();                    // 客户端断开
    void packetReceived(MsgType type, const QByteArray& body);  // 收到数据包
    void stateChanged(ConnectionState state);     // 状态变化
    void errorOccurred(const QString& message);   // 错误发生

private slots:
    void onNewConnection();                       // 新连接
    void onReadyRead();                           // 可读数据
    void onClientDisconnected();                  // 客户端断开
    void onSocketError(QAbstractSocket::SocketError error);
    void onPingTimeout();                          // 心跳超时

private:
    // ═══════════════════════════════════════════════════════════
    // 粘包处理（关键！）
    // ═══════════════════════════════════════════════════════════
    void tryParsePackets();

    // ═══════════════════════════════════════════════════════════
    // 重置状态
    // ═══════════════════════════════════════════════════════════
    void reset();

    QTcpServer*      m_server;
    QTcpSocket*      m_clientSocket;
    QByteArray       m_buffer;          // 粘包缓冲区
    ConnectionState  m_state;
    QTimer*          m_pingTimer;       // 心跳定时器
    QTimer*          m_timeoutTimer;    // 超时定时器
    quint64          m_lastPongTime;
};

// ═══════════════════════════════════════════════════════════════
// 便捷别名
// ═══════════════════════════════════════════════════════════════
using HostServer = GameServer;

} // namespace network
} // namespace game

#endif // GAME_SERVER_H
