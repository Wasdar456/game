#ifndef GAMEPROJECT_CORE_UNITS_ATTACKUNIT_H
#define GAMEPROJECT_CORE_UNITS_ATTACKUNIT_H

#include "core/units/Card.h"

namespace game::core {

// 攻击型卡牌。
// 自动技能会在冷却完成时选择范围内最高优先级怪物并造成伤害。
class AttackUnit : public Card {
public:
    explicit AttackUnit(int id, MapPosition position);

    // allies 当前未使用，保留统一技能接口，方便未来做联动技能。
    void autoSkill(std::vector<std::shared_ptr<Entity>>& allies,
                   std::vector<std::shared_ptr<Entity>>& enemies,
                   Map& map,
                   ResourceManager& resources) override;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_UNITS_ATTACKUNIT_H
