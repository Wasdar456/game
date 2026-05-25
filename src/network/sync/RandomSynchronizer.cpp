#include <network/sync/RandomSynchronizer.h>
#include <network/session/GameServer.h>
#include <network/protocol/Serializer.h>
#include <QRandomGenerator>
#include <QDebug>

namespace game {
namespace network {

RandomSynchronizer::RandomSynchronizer(QObject* parent)
    : QObject(parent)
    , m_seed(0)
{
    // 用时间初始化（后面会被 sync 覆盖）
    m_rng.seed(QRandomGenerator::global()->generate());
}

// ═══════════════════════════════════════════════════════════════
// Host：生成种子，发送给对方
// ═══════════════════════════════════════════════════════════════
quint32 RandomSynchronizer::generateAndSyncSeed(GameServer* server) {
    // 生成 32 位随机种子
    m_seed = QRandomGenerator::global()->generate();

    // 序列化并发送
    QByteArray body = Serializer::encodeUint32(m_seed);
    server->sendPacket(MsgType::SYNC_SEED, body);

    // 应用到本地 rng
    m_rng.seed(m_seed);

    qInfo() << "[RandomSynchronizer] Host 生成了种子:" << m_seed;
    emit seedSynced(m_seed);

    return m_seed;
}

// ═══════════════════════════════════════════════════════════════
// Client：应用收到的种子
// ═══════════════════════════════════════════════════════════════
void RandomSynchronizer::applyReceivedSeed(quint32 seed) {
    m_seed = seed;
    m_rng.seed(m_seed);

    qInfo() << "[RandomSynchronizer] Client 应用了种子:" << m_seed;
    emit seedSynced(m_seed);
}

// ═══════════════════════════════════════════════════════════════
// 从 body 解析种子
// ═══════════════════════════════════════════════════════════════
quint32 RandomSynchronizer::parseSeedFromBody(const QByteArray& body) {
    if (body.size() < 4) {
        qWarning() << "[RandomSynchronizer] 种子数据不完整:" << body.size();
        return 0;
    }

    quint32 seed;
    memcpy(&seed, body.constData(), 4);
    return qFromBigEndian(seed);
}

} // namespace network
} // namespace game
