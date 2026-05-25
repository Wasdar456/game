#include <network/session/RoundManager.h>
#include <QDebug>

namespace game {
namespace network {

// ─────────────────────────────────────────────────────────────
// 构造函数
// ─────────────────────────────────────────────────────────────
RoundManager::RoundManager(Role role, QObject* parent)
    : QObject(parent)
    , m_role(role)
    , m_hostAcked(false)
    , m_clientAcked(false)
    , m_waitingForAcks(false)
{}

void RoundManager::setRole(Role role) {
    m_role = role;
}

// ─────────────────────────────────────────────────────────────
// Host 专用：广播一个值
// ─────────────────────────────────────────────────────────────
void RoundManager::sendRoundValue(const QByteArray& value) {
    if (m_role != Role::Host) {
        qWarning() << "[RoundManager] 只有 Host 才能调用 sendRoundValue()";
        return;
    }
    if (m_waitingForAcks) {
        qWarning() << "[RoundManager] 上一轮还未完成，不能发下一轮"
                   << "| host=" << m_hostAcked
                   << "| client=" << m_clientAcked;
        return;
    }

    // 重置状态
    m_currentValue   = value;
    m_hostAcked      = false;
    m_clientAcked    = false;
    m_waitingForAcks = true;

    qInfo() << "[RoundManager][Host] ===== 广播新值，等待双方 ack =====" << value;

    // 1. 发 ROUND_VALUE 给 Client
    emit sendPacketRequested(MsgType::ROUND_VALUE, value);

    // 2. Host 自己也"收到"这个值（与 Client 对等）
    emit valueReceived(value);
}

// ─────────────────────────────────────────────────────────────
// 双方通用：本地处理完毕
// ─────────────────────────────────────────────────────────────
void RoundManager::localAck() {
    if (!m_waitingForAcks) {
        qWarning() << "[RoundManager] 当前没有等待 ack，localAck() 调用多余";
        return;
    }

    if (m_role == Role::Host) {
        if (m_hostAcked) {
            qWarning() << "[RoundManager][Host] 已经 ack 过了，重复调用";
            return;
        }
        m_hostAcked = true;
        qInfo() << "[RoundManager][Host] 本地 ack 完成";
        checkAllAcked();

    } else {
        // Client：发 ROUND_ACK 给 Host
        qInfo() << "[RoundManager][Client] 本地 ack 完成，发送 ROUND_ACK";
        emit sendPacketRequested(MsgType::ROUND_ACK, {});
        // Client 不需要等 ROUND_COMPLETE 才算完成
        // 等 Host 广播 ROUND_COMPLETE 后再触发 roundComplete
    }
}

// ─────────────────────────────────────────────────────────────
// 被外部调用：处理收到的网络包
// ─────────────────────────────────────────────────────────────
void RoundManager::onPacketReceived(MsgType type, const QByteArray& body) {
    switch (type) {

    // Client 收到 Host 广播的值
    case MsgType::ROUND_VALUE:
        if (m_role == Role::Client) {
            m_currentValue   = body;
            m_waitingForAcks = true;
            qInfo() << "[RoundManager][Client] 收到 ROUND_VALUE，大小=" << body.size();
            emit valueReceived(body);
        }
        break;

    // Host 收到 Client 的确认
    case MsgType::ROUND_ACK:
        if (m_role == Role::Host) {
            if (m_clientAcked) {
                qWarning() << "[RoundManager][Host] Client 重复 ack";
                return;
            }
            m_clientAcked = true;
            qInfo() << "[RoundManager][Host] 收到 Client 的 ROUND_ACK";
            checkAllAcked();
        }
        break;

    // Client 收到 Host 广播的"本轮完成"
    case MsgType::ROUND_COMPLETE:
        if (m_role == Role::Client) {
            m_waitingForAcks = false;
            qInfo() << "[RoundManager][Client] 收到 ROUND_COMPLETE，本轮完成";
            emit roundComplete();
        }
        break;

    default:
        break;
    }
}

// ─────────────────────────────────────────────────────────────
// Host 内部：检查双方是否都完成
// ─────────────────────────────────────────────────────────────
void RoundManager::checkAllAcked() {
    if (!m_hostAcked || !m_clientAcked) {
        qDebug() << "[RoundManager][Host] 等待中..."
                 << "host=" << m_hostAcked
                 << "client=" << m_clientAcked;
        return;
    }

    // 双方都 ack 了
    m_waitingForAcks = false;

    qInfo() << "[RoundManager][Host] ===== 双方都 ack，广播 ROUND_COMPLETE =====";

    // 广播 ROUND_COMPLETE 给 Client
    emit sendPacketRequested(MsgType::ROUND_COMPLETE, {});

    // 触发信号（顺序：先 allAcked，再 roundComplete）
    emit allAcked();
    emit roundComplete();
}

} // namespace network
} // namespace game
