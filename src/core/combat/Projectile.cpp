#include "core/combat/Projectile.h"
#include <algorithm>
#include <cmath>

namespace game::core {

Projectile::Projectile(int sourceId,
                       std::shared_ptr<Entity> target,
                       MapPosition from,
                       int damage,
                       ProjectileKind kind,
                       ProjectileOwner owner,
                       double speed,
                       int splashRadius)
    : sourceId_(sourceId),
      targetId_(target ? target->id() : -1),
      target_(target),
      from_(from),
      to_(target ? target->position() : from),
      damage_(damage),
      kind_(kind),
      owner_(owner),
      speed_(speed),
      splashRadius_(splashRadius),
      progress_(0.0),
      reached_(false) {}

void Projectile::update(double deltaSeconds) {
    if (reached_) return;
    if (auto target = target_.lock()) {
        to_ = target->position();
    }
    const double dr = static_cast<double>(to_.row - from_.row);
    const double dc = static_cast<double>(to_.col - from_.col);
    const double distance = std::max(1.0, std::sqrt(dr * dr + dc * dc));
    progress_ = std::min(1.0, progress_ + (speed_ * deltaSeconds / distance));
    reached_ = progress_ >= 1.0;
}

} // namespace game::core
