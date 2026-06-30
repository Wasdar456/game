#ifndef GAMEPROJECT_CORE_UNITS_ATTACKUNIT_H
#define GAMEPROJECT_CORE_UNITS_ATTACKUNIT_H

#include "core/units/Card.h"

namespace game::core {

// 攻击型卡牌。
// 自动技能会在冷却完成时选择范围内最高优先级怪物并造成伤害。
class AttackUnit : public Card {
public:
    explicit AttackUnit(int id, MapPosition position,
                        CardKind kind = CardKind::Attack);
    AttackUnit(int id,
               MapPosition position,
               int maxHp,
               int attack,
               int attackRange,
               int moveLimit,
               double skillCooldownSeconds,
               int deployCost,
               CardKind kind = CardKind::Attack,
               ProjectileKind projectileKind = ProjectileKind::Bullet,
               int splashRadius = 0);

    // allies 当前未使用，保留统一技能接口，方便未来做联动技能。
    void autoSkill(std::vector<std::shared_ptr<Entity>>& allies,
                   std::vector<std::shared_ptr<Entity>>& enemies,
                   Map& map,
                   ResourceManager& resources,
                   std::vector<Projectile>* projectiles = nullptr,
                   ProjectileOwner projectileOwner = ProjectileOwner::PlayerA) override;

private:
    ProjectileKind projectileKind_;
    int splashRadius_;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_UNITS_ATTACKUNIT_H
