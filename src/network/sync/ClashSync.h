#ifndef CLASH_SYNC_H
#define CLASH_SYNC_H

#include "../protocol/ProtocolDef.h"
#include <QObject>

namespace game {
namespace network {

// ═══════════════════════════════════════════════════════════════
// 拼点结果结构
// ═══════════════════════════════════════════════════════════════
struct ClashResult {
    int attackerUnitId;   // 发起瞬移的单位ID
    int defenderUnitId;   // 被踩的单位ID
    bool attackerWon;     // 发起方是否胜利
    int damageToWinner;   // 胜者受到的伤害
};

// ═══════════════════════════════════════════════════════════════
// ClashSync - 拼点踩死同步
//
// 当单位 A 瞬移到单位 B 所在格子时触发，
// 双方独立计算后取 Host 结果为准。
// ═══════════════════════════════════════════════════════════════
class ClashSync : public QObject {
    Q_OBJECT

public:
    explicit ClashSync(QObject* parent = nullptr);

    // ═══════════════════════════════════════════════════════════
    // 计算拼点结果（基于单位属性）
    // ═══════════════════════════════════════════════════════════
    ClashResult calculateClash(
        int attackerId, int attackerAtk, int attackerHp, int attackerMaxHp,
        int defenderId, int defenderAtk, int defenderHp, int defenderMaxHp
    );

    // ═══════════════════════════════════════════════════════════
    // 序列化拼点结果
    // ═══════════════════════════════════════════════════════════
    static QByteArray serializeClashResult(const ClashResult& result);

    // ═══════════════════════════════════════════════════════════
    // 解析拼点结果
    // ═══════════════════════════════════════════════════════════
    static ClashResult parseClashResult(const QByteArray& body);

signals:
    void clashOccurred(const ClashResult& result);

private:
    // ═══════════════════════════════════════════════════════════
    // 战斗值计算公式：(攻击力 + 剩余血量百分比 * 100)
    // ═══════════════════════════════════════════════════════════
    int calculateCombatPower(int attack, int currentHp, int maxHp) {
        int hpPercent = (maxHp > 0) ? (currentHp * 100 / maxHp) : 0;
        return attack + hpPercent;
    }
};

} // namespace network
} // namespace game

#endif // CLASH_SYNC_H
