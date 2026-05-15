#include "core/combat/Projectile.h"
#include <algorithm>

namespace game::core {

Projectile::Projectile(int sourceId, int targetId, MapPosition from,
                       MapPosition to, int damage, double speed)
    : sourceId_(sourceId),
      targetId_(targetId),
      from_(from),
      to_(to),
      damage_(damage),
      speed_(speed),
      progress_(0.0),
      reached_(false) {}

void Projectile::update(double deltaSeconds) {
    if (reached_) return;
    progress_ = std::min(1.0, progress_ + speed_ * deltaSeconds);
    reached_ = progress_ >= 1.0;
}

} // namespace game::core
