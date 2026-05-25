#include <network/session/LobbyManager.h>
#include <QDebug>
#include <QRandomGenerator>
#include <QtEndian>

namespace game {
namespace network {

LobbyManager::LobbyManager(Role role, const QString& nickname, QObject* parent)
    : QObject(parent)
    , m_role(role)
    , m_state(LobbyState::Idle)
    , m_myNickname(nickname)
    , m_localReady(false)
    , m_peerReady(false)
{}

// ─────────────────────────────────────────────────────────────
// 对端连接建立（Client 调用：连接成功后）
// ─────────────────────────────────────────────────────────────
void LobbyManager::onPeerConnected() {
    if (m_role == Role::Client) {
        // Client 主动发 JOIN_ROOM，携带自己的昵称
        QByteArray body = m_myNickname.toUtf8();
        qInfo() << "[Lobby][Client] 发送 JOIN_ROOM，昵称：" << m_myNickname;
        emit sendPacketRequested(MsgType::JOIN_ROOM, body);
        setState(LobbyState::Waiting);
    }
    // Host 侧不需要主动发，等待 Client 的 JOIN_ROOM
    if (m_role == Role::Host) {
        setState(LobbyState::Waiting);
    }
}

// ─────────────────────────────────────────────────────────────
// 收到网络包（外部路由进来）
// ─────────────────────────────────────────────────────────────
void LobbyManager::onPacketReceived(MsgType type, const QByteArray& body) {
    switch (type) {
    case MsgType::JOIN_ROOM:      handleJoinRoom(body);   break;
    case MsgType::JOIN_ACK:       handleJoinAck(body);    break;
    case MsgType::PLAYER_READY:   handlePlayerReady(body);break;
    case MsgType::PLAYER_UNREADY: handlePlayerUnready();  break;
    case MsgType::GAME_START:     handleGameStart(body);  break;
    default: break;
    }
}

// ─────────────────────────────────────────────────────────────
// 本地玩家点击"准备"
// ─────────────────────────────────────────────────────────────
void LobbyManager::setReady() {
    if (m_localReady) return;
    if (m_state == LobbyState::InGame) return;

    m_localReady = true;
    setState(LobbyState::LocalReady);

    qInfo() << "[Lobby]" << (m_role == Role::Host ? "[Host]" : "[Client]")
            << m_myNickname << "点击准备";

    emit sendPacketRequested(MsgType::PLAYER_READY, {});

    // Host 收到自己 ready 后检查
    if (m_role == Role::Host) {
        checkBothReady();
    }
}

// ─────────────────────────────────────────────────────────────
// 本地玩家取消准备
// ─────────────────────────────────────────────────────────────
void LobbyManager::cancelReady() {
    if (!m_localReady) return;
    if (m_state == LobbyState::InGame) return;

    m_localReady = false;
    setState(LobbyState::Connected);

    qInfo() << "[Lobby] 取消准备";
    emit sendPacketRequested(MsgType::PLAYER_UNREADY, {});
}

// ─────────────────────────────────────────────────────────────
// Host 收到 JOIN_ROOM
// ─────────────────────────────────────────────────────────────
void LobbyManager::handleJoinRoom(const QByteArray& body) {
    if (m_role != Role::Host) return;

    m_peerNickname = QString::fromUtf8(body);
    qInfo() << "[Lobby][Host] 收到 JOIN_ROOM，对方昵称：" << m_peerNickname;

    // 回复 JOIN_ACK，携带房主昵称
    QByteArray ackBody = m_myNickname.toUtf8();
    emit sendPacketRequested(MsgType::JOIN_ACK, ackBody);

    setState(LobbyState::Connected);
    emit peerJoined(m_peerNickname);
}

// ─────────────────────────────────────────────────────────────
// Client 收到 JOIN_ACK
// ─────────────────────────────────────────────────────────────
void LobbyManager::handleJoinAck(const QByteArray& body) {
    if (m_role != Role::Client) return;

    m_peerNickname = QString::fromUtf8(body);
    qInfo() << "[Lobby][Client] 收到 JOIN_ACK，房主昵称：" << m_peerNickname;

    setState(LobbyState::Connected);
    emit peerJoined(m_peerNickname);
}

// ─────────────────────────────────────────────────────────────
// 收到对方 PLAYER_READY
// ─────────────────────────────────────────────────────────────
void LobbyManager::handlePlayerReady(const QByteArray& /*body*/) {
    m_peerReady = true;
    qInfo() << "[Lobby] 对方" << m_peerNickname << "已准备";

    emit peerReady();

    if (m_role == Role::Host) {
        checkBothReady();
    }
}

// ─────────────────────────────────────────────────────────────
// 收到对方 PLAYER_UNREADY
// ─────────────────────────────────────────────────────────────
void LobbyManager::handlePlayerUnready() {
    m_peerReady = false;
    qInfo() << "[Lobby] 对方" << m_peerNickname << "取消准备";
    setState(LobbyState::Connected);
    emit peerCancelled();
}

// ─────────────────────────────────────────────────────────────
// Client 收到 GAME_START（携带种子）
// ─────────────────────────────────────────────────────────────
void LobbyManager::handleGameStart(const QByteArray& body) {
    if (m_role != Role::Client) return;

    quint32 seed = 0;
    if (body.size() >= 4) {
        memcpy(&seed, body.constData(), 4);
        seed = qFromBigEndian(seed);
    }

    qInfo() << "[Lobby][Client] 收到 GAME_START，种子=" << seed;
    setState(LobbyState::InGame);
    emit gameStarted(seed);
}

// ─────────────────────────────────────────────────────────────
// Host：检查双方是否都准备好
// ─────────────────────────────────────────────────────────────
void LobbyManager::checkBothReady() {
    if (!m_localReady || !m_peerReady) {
        qDebug() << "[Lobby][Host] 等待双方准备..."
                 << "| 我:" << (m_localReady ? "✓" : "✗")
                 << "| 对方:" << (m_peerReady ? "✓" : "✗");
        return;
    }

    // 生成随机种子
    quint32 seed = QRandomGenerator::global()->generate();
    qInfo() << "[Lobby][Host] ===== 双方都准备好！生成种子=" << seed << "，广播 GAME_START =====";

    // 种子以大端序写入 body
    QByteArray body(4, 0);
    qToBigEndian(seed, reinterpret_cast<uchar*>(body.data()));

    emit sendPacketRequested(MsgType::GAME_START, body);

    setState(LobbyState::InGame);
    emit gameStarted(seed);
}

// ─────────────────────────────────────────────────────────────
// 更新状态机
// ─────────────────────────────────────────────────────────────
void LobbyManager::setState(LobbyState s) {
    if (m_state == s) return;
    m_state = s;
    emit stateChanged(s);
}

} // namespace network
} // namespace game
