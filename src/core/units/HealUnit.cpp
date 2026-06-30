#include "core/units/HealUnit.h"
#include "core/base/Constants.h"

namespace game::core {

HealUnit::HealUnit(int id, MapPosition position, CardKind kind)
    : Card(id, position, kind, ObjectType::CardHeal, 60, 0, 2, 2, 2.0,
           constants::DeployCostHeal),
      healAmount_(15) {
    priorityList_ = {
        ObjectType::CardAttack,
        ObjectType::CardProduce,
        ObjectType::CardHeal
    };
}

HealUnit::HealUnit(int id,
                   MapPosition position,
                   int maxHp,
                   int attackRange,
                   int moveLimit,
                   double skillCooldownSeconds,
                   int deployCost,
                   CardKind kind,
                   int healAmount)
    : Card(id, position, kind, ObjectType::CardHeal, maxHp, 0, attackRange, moveLimit,
           skillCooldownSeconds, deployCost),
      healAmount_(healAmount) {
    priorityList_ = {
        ObjectType::CardAttack,
        ObjectType::CardProduce,
        ObjectType::CardHeal
    };
}

void HealUnit::autoSkill(std::vector<std::shared_ptr<Entity>>& allies,
                         std::vector<std::shared_ptr<Entity>>&,
                         Map& map,
                         ResourceManager&,
                         std::vector<Projectile>*,
                         ProjectileOwner) {
    if (!isSkillReady()) return;

    // 治疗模式只选择范围内未满血友军。
    auto target = findTarget(allies, map, true);
    if (!target) return;

    // 治疗量随等级提升。
    target->heal(healAmount_ + (level_ - 1) * 8);
    resetCooldown();
}

} // namespace game::core
