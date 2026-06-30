#include "core/units/ProduceUnit.h"
#include "core/base/Constants.h"
#include "core/systems/ResourceManager.h"

namespace game::core {

ProduceUnit::ProduceUnit(int id, MapPosition position, CardKind kind)
    : Card(id, position, kind, ObjectType::CardProduce, 80, 0, 0, 1, 5.0,
           constants::DeployCostProduce),
      resourceYield_(25) {}

ProduceUnit::ProduceUnit(int id,
                         MapPosition position,
                         int maxHp,
                         int moveLimit,
                         double skillCooldownSeconds,
                         int deployCost,
                         CardKind kind,
                         int resourceYield)
    : Card(id, position, kind, ObjectType::CardProduce, maxHp, 0, 0, moveLimit,
           skillCooldownSeconds, deployCost),
      resourceYield_(resourceYield) {}

void ProduceUnit::autoSkill(std::vector<std::shared_ptr<Entity>>&,
                            std::vector<std::shared_ptr<Entity>>&,
                            Map&,
                            ResourceManager& resources,
                            std::vector<Projectile>*,
                            ProjectileOwner) {
    // 生产单位不需要目标，只要冷却完成就产出资源。
    if (!isSkillReady()) return;

    // 等级越高，额外产出越多。
    resources.addResource(resourceYield_ + (level_ - 1) * 10);
    resetCooldown();
}

} // namespace game::core
