#ifndef GAMEPROJECT_CORE_UNITS_HEALUNIT_H
#define GAMEPROJECT_CORE_UNITS_HEALUNIT_H

#include "core/units/Card.h"

namespace game::core {

// 治疗型卡牌。
// 冷却完成后自动寻找范围内受伤友方，优先治疗血量百分比最低者。
class HealUnit : public Card {
public:
    explicit HealUnit(int id, MapPosition position);
    HealUnit(int id,
             MapPosition position,
             int maxHp,
             int attackRange,
             int moveLimit,
             double skillCooldownSeconds,
             int deployCost,
             int healAmount);

    int healAmount() const { return healAmount_; }

    // enemies 当前未使用，保留统一技能接口。
    void autoSkill(std::vector<std::shared_ptr<Entity>>& allies,
                   std::vector<std::shared_ptr<Entity>>& enemies,
                   Map& map,
                   ResourceManager& resources,
                   std::vector<Projectile>* projectiles = nullptr,
                   ProjectileOwner projectileOwner = ProjectileOwner::PlayerA) override;

private:
    // 基础治疗量。
    int healAmount_;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_UNITS_HEALUNIT_H
