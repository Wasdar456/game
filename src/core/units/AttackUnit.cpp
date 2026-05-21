#include "core/units/AttackUnit.h"
#include "core/base/Constants.h"
#include "core/combat/DamageCalculator.h"

namespace game::core {

AttackUnit::AttackUnit(int id, MapPosition position)
    : Card(id, position, ObjectType::CardAttack, 100, 20, 3, 2, 1.0,
           constants::DeployCostAttack) {
    priorityList_ = {
        ObjectType::MonsterAtkSapper,
        ObjectType::MonsterAtkRanged,
        ObjectType::MonsterAtkTank,
        ObjectType::MonsterAtkNormal,
        ObjectType::MonsterAtkFast,
        ObjectType::MonsterAtkBerserk,
        ObjectType::MonsterAtkRegen,
        ObjectType::MonsterResTank,
        ObjectType::MonsterResBasic,
        ObjectType::MonsterResFast
    };
}

void AttackUnit::autoSkill(std::vector<std::shared_ptr<Entity>>&,
                           std::vector<std::shared_ptr<Entity>>& enemies,
                           Map& map,
                           ResourceManager&) {
    // 冷却未完成时不做任何事，符合自动技能模型。
    if (!isSkillReady()) return;

    // 按优先级、距离、血量和 id 选择目标。
    auto target = findTarget(enemies, map);
    if (!target) return;

    // 伤害计算集中交给 DamageCalculator，自动应用高低差惩罚。
    target->takeDamage(DamageCalculator::calculateDamage(*this, *target, map));
    resetCooldown();
}

} // namespace game::core
