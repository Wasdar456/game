#include "core/combat/DamageCalculator.h"
#include <algorithm>
#include <cmath>

namespace game::core {

int DamageCalculator::calculateDamage(const Entity& attacker,
                                      const Entity& target,
                                      const Map& map) {
    // 基础伤害来自攻击者攻击力。负数攻击会被视为 0。
    int damage = std::max(0, attacker.attack());
    const MapGrid* attackerGrid = map.gridAt(attacker.position());
    const MapGrid* targetGrid = map.gridAt(target.position());

    // README 中的高低差规则：低处攻击高处时最终伤害降低。
    if (attackerGrid && targetGrid &&
        attackerGrid->height() < targetGrid->height()) {
        damage = static_cast<int>(std::round(
            damage * constants::LowGroundDamageMultiplier));
    }

    return std::max(0, damage);
}

int DamageCalculator::combatPower(int attack, int currentHp, int maxHp) {
    // 剩余血量百分比使用整数 0..100，保证 PVP 双端确定性一致。
    int hpPercent = maxHp > 0 ? (currentHp * 100 / maxHp) : 0;
    return attack + hpPercent;
}

ClashOutcome DamageCalculator::calculateClash(const Entity& attacker,
                                              const Entity& defender) {
    ClashOutcome outcome;
    outcome.attackerId = attacker.id();
    outcome.defenderId = defender.id();

    int attackerPower = combatPower(attacker.attack(), attacker.hp(), attacker.maxHp());
    int defenderPower = combatPower(defender.attack(), defender.hp(), defender.maxHp());

    // 平局判给防守方，降低主动踩人策略的无脑收益。
    outcome.attackerWon = attackerPower > defenderPower;
    outcome.damageToWinner = outcome.attackerWon
        ? defender.maxHp() / 2
        : attacker.maxHp() / 2;
    return outcome;
}

} // namespace game::core
