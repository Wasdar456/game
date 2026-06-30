#ifndef GAMEPROJECT_CORE_COMBAT_PROJECTILE_H
#define GAMEPROJECT_CORE_COMBAT_PROJECTILE_H

#include "core/map/MapPosition.h"
#include "core/base/Entity.h"
#include <memory>

namespace game::core {

enum class ProjectileKind : std::uint8_t {
    Bullet = 0,
    Sniper,
    Aoe,
    Monster
};

enum class ProjectileOwner : std::uint8_t {
    PlayerA = 0,
    PlayerB,
    Monster
};

// 投射物数据模型。伤害在投射物抵达后由 BattleManager 统一结算。
class Projectile {
public:
    Projectile(int sourceId,
               std::shared_ptr<Entity> target,
               MapPosition from,
               int damage,
               ProjectileKind kind,
               ProjectileOwner owner,
               double speed = 8.0,
               int splashRadius = 0);

    int sourceId() const { return sourceId_; }
    int targetId() const { return targetId_; }
    int damage() const { return damage_; }
    bool reached() const { return reached_; }
    double progress() const { return progress_; }
    MapPosition from() const { return from_; }
    MapPosition to() const { return to_; }
    ProjectileKind kind() const { return kind_; }
    ProjectileOwner owner() const { return owner_; }
    int splashRadius() const { return splashRadius_; }
    std::shared_ptr<Entity> target() const { return target_.lock(); }

    // 根据速度推进飞行进度，progress 到 1.0 时视为到达目标。
    void update(double deltaSeconds);

private:
    int sourceId_;
    int targetId_;
    std::weak_ptr<Entity> target_;
    MapPosition from_;
    MapPosition to_;
    int damage_;
    ProjectileKind kind_;
    ProjectileOwner owner_;
    double speed_;
    int splashRadius_;
    double progress_;
    bool reached_;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_COMBAT_PROJECTILE_H
