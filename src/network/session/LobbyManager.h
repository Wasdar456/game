#ifndef LOBBY_MANAGER_H
#define LOBBY_MANAGER_H

#include "../protocol/ProtocolDef.h"
#include <QObject>
#include <QString>
#include <QByteArray>

namespace game {
namespace network {

// ═══════════════════════════════════════════════════════════════
// LobbyManager - 大厅握手流程管理
//
// 负责游戏开始前的握手：
//   1. Client 连接后发 JOIN_ROOM（携带昵称）
//   2. Host 收到 JOIN_ROOM 后回复 JOIN_ACK（携带房主昵称）
//   3. 任意一方调用 setReady() → 发 PLAYER_READY
//   4. Host 收到 Client 的 PLAYER_READY，且自己也 ready
//      → 生成随机数种子，广播 GAME_START（携带种子和地图 id）
//   5. 双方触发 gameStarted(seed, mapId) 信号，进入游戏主循环
//
// 使用方法：
//   - Host: LobbyManager lobby(LobbyManager::Role::Host, "我的昵称");
//           连接到 GameServer 的 packetReceived + clientConnected
//           调用 lobby.setReady() 表示房主准备好了
//   - Client: LobbyManager lobby(LobbyManager::Role::Client, "我的昵称");
//             连接到 GameClient 的 packetReceived + connected
//             调用 lobby.setReady() 表示玩家准备好了
// ═══════════════════════════════════════════════════════════════
class LobbyManager : public QObject {
    Q_OBJECT

public:
    enum class Role { Host, Client };

    // 大厅状态机
    enum class LobbyState {
        Idle,           // 初始状态
        Waiting,        // Host: 等待 Client 连接 / Client: 已连接等待 JOIN_ACK
        Connected,      // 双方建立连接，等待双方准备
        LocalReady,     // 本地已准备，等待对方
        AllReady,       // 双方都准备好（仅 Host 判断后触发）
        InGame          // 游戏进行中
    };

    explicit LobbyManager(Role role, const QString& nickname, QObject* parent = nullptr);

    // ─── 状态查询 ───
    Role       role()         const { return m_role; }
    LobbyState state()        const { return m_state; }
    QString    myNickname()   const { return m_myNickname; }
    QString    peerNickname() const { return m_peerNickname; }
    bool       isReady()      const { return m_localReady; }
    bool       peerIsReady()  const { return m_peerReady; }
    QString    selectedMapId() const { return m_selectedMapId; }

    // ─── 外部调用接口 ───

    // 客户端连接建立后调用（发送 JOIN_ROOM）
    // 通常连接到 GameClient::connected 信号
    void onPeerConnected();

    // 收到对方网络包（连接到 Server/Client 的 packetReceived 信号）
    void onPacketReceived(MsgType type, const QByteArray& body);

    // 玩家点击"准备"按钮
    void setReady();
    void setSelectedMapId(const QString& mapId);

    // 玩家取消准备
    void cancelReady();

signals:
    // ─── 请求发包（连接到 GameServer/GameClient 的 sendPacket）───
    void sendPacketRequested(MsgType type, const QByteArray& body);

    // ─── 大厅事件 ───

    // 对方加入了房间（携带对方昵称）
    void peerJoined(const QString& peerNickname);

    // 对方点了准备
    void peerReady();

    // 对方取消了准备
    void peerCancelled();

    // 状态变化
    void stateChanged(LobbyState newState);

    // ─── 最终结果 ───

    // 游戏正式开始（双方都触发，携带随机数种子和地图 id）
    void gameStarted(quint32 seed, const QString& mapId);

private:
    void handleJoinRoom(const QByteArray& body);   // Host 收到 JOIN_ROOM
    void handleJoinAck(const QByteArray& body);    // Client 收到 JOIN_ACK
    void handlePlayerReady(const QByteArray& body);// 收到对方 READY
    void handlePlayerUnready();                    // 收到对方取消 READY
    void handleGameStart(const QByteArray& body);  // Client 收到 GAME_START

    void checkBothReady();   // Host: 检查是否双方都 ready，是则广播 GAME_START
    void setState(LobbyState s);

    Role        m_role;
    LobbyState  m_state;
    QString     m_myNickname;
    QString     m_peerNickname;
    QString     m_selectedMapId;
    bool        m_localReady;
    bool        m_peerReady;
};

} // namespace network
} // namespace game

#endif // LOBBY_MANAGER_H
