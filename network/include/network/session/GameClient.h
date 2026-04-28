#ifndef GAME_CLIENT_H
#define GAME_CLIENT_H

#include "../protocol/ProtocolDef.h"
#include "NetworkState.h"
#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QByteArray>

namespace game {
namespace network {

// ═══════════════════════════════════════════════════════════════
// GameClient - Client 端：连接服务器
// ═══════════════════════════════════════════════════════════════
class GameClient : public QObject {
    Q_OBJECT

public:
    explicit GameClient(QObject* parent = nullptr);
    ~GameClient();

    // ═══════════════════════════════════════════════════════════
    // 连接服务器
    // ═══════════════════════════════════════════════════════════
    void connectToHost(const QString& ip, quint16 port = 9527);

    // ═══════════════════════════════════════════════════════════
    // 断开连接
    // ═══════════════════════════════════════════════════════════
    void disconnect();

    // ═══════════════════════════════════════════════════════════
    // 发送数据包
    // ═══════════════════════════════════════════════════════════
    void sendPacket(MsgType type, const QByteArray& body = {});

    // ═══════════════════════════════════════════════════════════
    // 获取状态
    // ═══════════════════════════════════════════════════════════
    ConnectionState state() const { return m_state; }
    QString serverIp() const { return m_serverIp; }
    quint16 serverPort() const { return m_serverPort; }

signals:
    // ═══════════════════════════════════════════════════════════
    // 信号定义
    // ═══════════════════════════════════════════════════════════
    void connected();                          // 连接成功
    void disconnected();                       // 断开连接
    void packetReceived(MsgType type, const QByteArray& body);  // 收到数据包
    void stateChanged(ConnectionState state); // 状态变化
    void errorOccurred(const QString& message); // 错误发生

private slots:
    void onConnected();                        // 连接成功
    void onReadyRead();                         // 可读数据
    void onDisconnected();                      // 断开连接
    void onSocketError(QAbstractSocket::SocketError error);
    void onPingTimeout();                       // 心跳超时

private:
    // ═══════════════════════════════════════════════════════════
    // 粘包处理
    // ═══════════════════════════════════════════════════════════
    void tryParsePackets();

    // ═══════════════════════════════════════════════════════════
    // 重置状态
    // ═══════════════════════════════════════════════════════════
    void reset();

    QTcpSocket*     m_socket;
    QByteArray      m_buffer;          // 粘包缓冲区
    ConnectionState m_state;
    QString         m_serverIp;
    quint16         m_serverPort;
    QTimer*         m_pingTimer;       // 心跳定时器
    QTimer*         m_timeoutTimer;    // 超时定时器
    quint64         m_lastPongTime;
};

// ═══════════════════════════════════════════════════════════════
// 便捷别名
// ═══════════════════════════════════════════════════════════════
using ClientSocket = GameClient;

} // namespace network
} // namespace game

#endif // GAME_CLIENT_H
