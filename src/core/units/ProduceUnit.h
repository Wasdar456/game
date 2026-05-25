#ifndef GAMEPROJECT_CORE_UNITS_PRODUCEUNIT_H
#define GAMEPROJECT_CORE_UNITS_PRODUCEUNIT_H

#include "core/units/Card.h"

namespace game::core {

// 生产型卡牌。
// 冷却完成后自动向 ResourceManager 增加资源。
class ProduceUnit : public Card {
public:
    explicit ProduceUnit(int id, MapPosition position);
    ProduceUnit(int id,
                MapPosition position,
                int maxHp,
                int moveLimit,
                double skillCooldownSeconds,
                int deployCost,
                int resourceYield);

    int resourceYield() const { return resourceYield_; }

    // 产出量会随等级略微提升。
    void autoSkill(std::vector<std::shared_ptr<Entity>>& allies,
                   std::vector<std::shared_ptr<Entity>>& enemies,
                   Map& map,
                   ResourceManager& resources,
                   std::vector<Projectile>* projectiles = nullptr,
                   ProjectileOwner projectileOwner = ProjectileOwner::PlayerA) override;

private:
    // 基础资源产出。
    int resourceYield_;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_UNITS_PRODUCEUNIT_H
