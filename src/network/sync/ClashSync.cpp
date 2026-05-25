#include <network/sync/ClashSync.h>
#include <network/protocol/Serializer.h>
#include <network/protocol/Deserializer.h>
#include <QDebug>

namespace game {
namespace network {

ClashSync::ClashSync(QObject* parent)
    : QObject(parent)
{}

// ═══════════════════════════════════════════════════════════════
// 计算拼点结果
//
// 战斗值 = 攻击力 + 剩余血量百分比 * 100
// 胜者保留但扣除败者 50% 血量的伤害
// ═══════════════════════════════════════════════════════════════
ClashResult ClashSync::calculateClash(
    int attackerId, int attackerAtk, int attackerHp, int attackerMaxHp,
    int defenderId, int defenderAtk, int defenderHp, int defenderMaxHp
) {
    ClashResult result;
    result.attackerUnitId = attackerId;
    result.defenderUnitId = defenderId;

    int attackerPower = calculateCombatPower(attackerAtk, attackerHp, attackerMaxHp);
    int defenderPower = calculateCombatPower(defenderAtk, defenderHp, defenderMaxHp);

    qDebug() << "[ClashSync] 拼点计算:"
             << "攻击方" << attackerId << "战力=" << attackerPower
             << "防御方" << defenderId << "战力=" << defenderPower;

    if (attackerPower > defenderPower) {
        result.attackerWon = true;
        result.damageToWinner = defenderMaxHp / 2;  // 胜者扣败者 50% 血量
    } else {
        result.attackerWon = false;
        result.damageToWinner = attackerMaxHp / 2;
    }

    emit clashOccurred(result);
    return result;
}

// ═══════════════════════════════════════════════════════════════
// 序列化拼点结果
// ═══════════════════════════════════════════════════════════════
QByteArray ClashSync::serializeClashResult(const ClashResult& result) {
    QByteArray body;
    body.reserve(9);

    body.append(Serializer::encodeUint32(static_cast<quint32>(result.attackerUnitId)));
    body.append(Serializer::encodeUint32(static_cast<quint32>(result.defenderUnitId)));
    body.append(Serializer::encodeUint8(result.attackerWon ? 1 : 0));
    body.append(Serializer::encodeUint32(static_cast<quint32>(result.damageToWinner)));

    return body;
}

// ═══════════════════════════════════════════════════════════════
// 解析拼点结果
// ═══════════════════════════════════════════════════════════════
ClashResult ClashSync::parseClashResult(const QByteArray& body) {
    ClashResult result;

    if (body.size() < 9) {
        qWarning() << "[ClashSync] 拼点数据不完整:" << body.size();
        return result;
    }

    Deserializer d(body);

    quint32 attackerId, defenderId, damage;
    quint8 won;

    d.decodeUint32(attackerId);
    d.decodeUint32(defenderId);
    d.decodeUint8(won);
    d.decodeUint32(damage);

    result.attackerUnitId = static_cast<int>(attackerId);
    result.defenderUnitId = static_cast<int>(defenderId);
    result.attackerWon = (won == 1);
    result.damageToWinner = static_cast<int>(damage);

    return result;
}

} // namespace network
} // namespace game
#include "ClashSync.moc"
