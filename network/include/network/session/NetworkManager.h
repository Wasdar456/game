#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "GameServer.h"
#include "GameClient.h"
#include "NetworkState.h"
#include <QObject>
#include <QTimer>

namespace game {
namespace network {

// ═══════════════════════════════════════════════════════════════
// NetworkManager - 网络状态机：统一管理 Server/Client
// ═══════════════════════════════════════════════════════════════
class NetworkManager : public QObject {
    Q_OBJECT

public:
    static NetworkManager* instance();

    // ═══════════════════════════════════════════════════════════
    // 创建房间（Host）
    // ═══════════════════════════════════════════════════════════
    bool createRoom(quint16 port = 9527);

    // ═══════════════════════════════════════════════════════════
    // 加入房间（Client）
    // ═══════════════════════════════════════════════════════════
    void joinRoom(const QString& hostIp, quint16 port = 9527);

    // ═══════════════════════════════════════════════════════════
    // 断开连接
    // ═══════════════════════════════════════════════════════════
    void disconnect();

    // ═══════════════════════════════════════════════════════════
    // 发送数据（自动选择 Server 或 Client）
    // ═══════════════════════════════════════════════════════════
    void sendPacket(MsgType type, const QByteArray& body = {});

    // ═══════════════════════════════════════════════════════════
    // 获取状态
    // ═══════════════════════════════════════════════════════════
    RoomRole role() const { return m_role; }
    ConnectionState state() const { return m_state; }
    QString getLocalIp() const;  // 获取本机局域网 IP

    // ═══════════════════════════════════════════════════════════
    // 获取 Server/Client 指针（用于直接操作）
    // ═══════════════════════════════════════════════════════════
    GameServer* server() { return m_server; }
    GameClient* client() { return m_client; }

signals:
    void stateChanged(ConnectionState state);
    void roleChanged(RoomRole role);
    void roomCreated(quint16 port);        // 房间创建成功
    void connectedToRoom();                // 加入房间成功
    void connectionFailed(const QString& reason);
    void disconnected();
    void packetReceived(MsgType type, const QByteArray& body);
    void errorOccurred(const QString& message);

private:
    explicit NetworkManager(QObject* parent = nullptr);
    ~NetworkManager();
    Q_DISABLE_COPY(NetworkManager)

    void setState(ConnectionState state);
    void setRole(RoomRole role);
    void connectServerSignals();
    void connectClientSignals();

    GameServer*      m_server;
    GameClient*      m_client;
    RoomRole          m_role;
    ConnectionState  m_state;
};

} // namespace network
} // namespace game

#endif // NETWORK_MANAGER_H
