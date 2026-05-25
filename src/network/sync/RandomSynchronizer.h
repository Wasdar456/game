#ifndef RANDOM_SYNCHRONIZER_H
#define RANDOM_SYNCHRONIZER_H

#include "../protocol/ProtocolDef.h"
#include "../session/GameServer.h"
#include <QObject>
#include <random>
#include <QtGlobal>

namespace game {
namespace network {

// ═══════════════════════════════════════════════════════════════
// RandomSynchronizer - 随机数种子同步器
//
// PVP 双方必须用同一个种子初始化 std::mt19937，
// 保证刷怪序列完全一致。
// ═══════════════════════════════════════════════════════════════
class RandomSynchronizer : public QObject {
    Q_OBJECT

public:
    explicit RandomSynchronizer(QObject* parent = nullptr);

    // ═══════════════════════════════════════════════════════════
    // Host 调用：生成种子，发送给对方
    // ═══════════════════════════════════════════════════════════
    quint32 generateAndSyncSeed(GameServer* server);

    // ═══════════════════════════════════════════════════════════
    // Client 调用：解析收到的种子，应用到本地
    // ═══════════════════════════════════════════════════════════
    void applyReceivedSeed(quint32 seed);

    // ═══════════════════════════════════════════════════════════
    // 从收到的 body 中解析种子
    // ═══════════════════════════════════════════════════════════
    static quint32 parseSeedFromBody(const QByteArray& body);

    // ═══════════════════════════════════════════════════════════
    // 获取本地 rng（用于生成怪物等）
    // ═══════════════════════════════════════════════════════════
    std::mt19937* rng() { return &m_rng; }

    // ═══════════════════════════════════════════════════════════
    // 生成指定范围的随机数
    // ═══════════════════════════════════════════════════════════
    template<typename T>
    T randomInt(T minVal, T maxVal) {
        std::uniform_int_distribution<T> dist(minVal, maxVal);
        return dist(m_rng);
    }

    quint32 currentSeed() const { return m_seed; }

signals:
    void seedSynced(quint32 seed);

private:
    std::mt19937 m_rng;
    quint32      m_seed;
};

} // namespace network
} // namespace game

#endif // RANDOM_SYNCHRONIZER_H
