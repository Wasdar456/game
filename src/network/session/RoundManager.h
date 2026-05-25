#ifndef ROUND_MANAGER_H
#define ROUND_MANAGER_H

#include "../protocol/ProtocolDef.h"
#include <QObject>
#include <QByteArray>

namespace game {
namespace network {

// ═══════════════════════════════════════════════════════════════
// RoundManager - 广播 + 双方确认机制
//
// 流程：
//   Host 调用 sendRoundValue(data)
//     → 广播 ROUND_VALUE 给 Client
//     → Host 自己触发 valueReceived 信号
//   上层逻辑处理完后，双方各自调用 localAck()
//     → Client: 发 ROUND_ACK 包给 Host
//     → Host:   本地标记已确认
//   Host 收到 Client 的 ROUND_ACK 且自己也确认后
//     → 广播 ROUND_COMPLETE（通知 Client 本轮结束）
//     → 触发 allAcked 信号，Host 可以调用下一次 sendRoundValue
// ═══════════════════════════════════════════════════════════════
class RoundManager : public QObject {
    Q_OBJECT

public:
    enum class Role { Host, Client };

    explicit RoundManager(Role role, QObject* parent = nullptr);

    // ─── 运行时切换角色 ───
    void setRole(Role role);
    Role role() const { return m_role; }

    // ─── Host 专用 ───
    // 广播一个值给双方（Host 自己也会触发 valueReceived 信号）
    void sendRoundValue(const QByteArray& value);

    // ─── 双方通用 ───
    // 上层逻辑"处理完当前值"后调用此函数
    // Host:   标记本地完成，检查是否双方都完成
    // Client: 发 ROUND_ACK 包给 Host
    void localAck();

    // ─── 被 NetworkManager 调用，转发网络包 ───
    void onPacketReceived(MsgType type, const QByteArray& body);

    // ─── 状态查询 ───
    bool isWaitingForAcks() const { return m_waitingForAcks; }
    bool hostAcked()   const { return m_hostAcked; }
    bool clientAcked() const { return m_clientAcked; }

signals:
    // ─────────────────────────────────────────────────────────
    // 收到下发的值（Host 和 Client 都会触发）
    // 上层处理完后必须调用 localAck()
    // ─────────────────────────────────────────────────────────
    void valueReceived(const QByteArray& value);

    // ─────────────────────────────────────────────────────────
    // 双方都 ack 了（仅 Host 触发，紧接 roundComplete 之前）
    // ─────────────────────────────────────────────────────────
    void allAcked();

    // ─────────────────────────────────────────────────────────
    // 本轮完成（Host 和 Client 都会触发）
    // Host:   在收到双方 ack 后广播 ROUND_COMPLETE，然后触发本信号
    // Client: 在收到 ROUND_COMPLETE 包后触发本信号
    // ─────────────────────────────────────────────────────────
    void roundComplete();

    // ─────────────────────────────────────────────────────────
    // 请求发包（连接到 GameServer/GameClient 的 sendPacket）
    // ─────────────────────────────────────────────────────────
    void sendPacketRequested(MsgType type, const QByteArray& body);

private:
    void checkAllAcked();   // Host 内部：检查两方是否都完成

    Role       m_role;
    bool       m_hostAcked;
    bool       m_clientAcked;
    bool       m_waitingForAcks;
    QByteArray m_currentValue;
};

} // namespace network
} // namespace game

#endif // ROUND_MANAGER_H
