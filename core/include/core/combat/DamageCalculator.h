#ifndef GAMEPROJECT_CORE_COMBAT_DAMAGECALCULATOR_H
#define GAMEPROJECT_CORE_COMBAT_DAMAGECALCULATOR_H

#include "core/base/Constants.h"
#include "core/base/Entity.h"
#include "core/map/Map.h"

namespace game::core {

// 拼点结果。用于 PVP “瞬移踩死”机制，也可给网络层序列化。
struct ClashOutcome {
    int attackerId = -1;
    int defenderId = -1;
    bool attackerWon = false;
    int damageToWinner = 0;
};

// 战斗数值计算工具类。
//
// 它不保存状态，只集中处理伤害、高低差和拼点公式，
// 这样 Card、Monster、BattleManager 都不会散落重复计算。
class DamageCalculator {
public:
    // 普通伤害公式：先取攻击者攻击力，再根据地图高度做低打高衰减。
    static int calculateDamage(const Entity& attacker,
                               const Entity& target,
                               const Map& map);

    // 拼点战斗值：攻击力 + 当前血量百分比。与 README 的 PVP 机制一致。
    static int combatPower(int attack, int currentHp, int maxHp);

    // 计算拼点胜负。平局默认防守方胜，避免主动踩人总是占优。
    static ClashOutcome calculateClash(const Entity& attacker,
                                       const Entity& defender);
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_COMBAT_DAMAGECALCULATOR_H
