#include "core/base/Entity.h"

namespace game::core {

Entity::Entity(int id, MapPosition position, ObjectType type, Team team,
               int maxHp, int attack)
    : GameObject(id, position, type),
      hp_(maxHp),
      maxHp_(maxHp),
      attack_(attack),
      team_(team) {}

float Entity::getHpPercent() const {
    // maxHp_ 理论上始终大于 0，这里仍然防御性处理，避免配置错误导致除零。
    return maxHp_ > 0 ? static_cast<float>(hp_) / static_cast<float>(maxHp_) : 0.0f;
}

void Entity::setMaxHp(int maxHp) {
    // 最大生命至少为 1，当前生命同步夹紧到合法范围。
    maxHp_ = std::max(1, maxHp);
    hp_ = std::clamp(hp_, 0, maxHp_);
}

void Entity::setHp(int hp) {
    hp_ = std::clamp(hp, 0, maxHp_);
}

void Entity::takeDamage(int damage) {
    // 死亡实体不再受伤，非正数伤害也被忽略。
    if (damage <= 0 || isDead()) return;
    hp_ = std::max(0, hp_ - damage);
}

void Entity::heal(int amount) {
    // 死亡实体不被治疗；治疗量不会超过最大生命。
    if (amount <= 0 || isDead()) return;
    hp_ = std::min(maxHp_, hp_ + amount);
}

} // namespace game::core
