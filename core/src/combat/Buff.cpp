#include "core/combat/Buff.h"
#include <algorithm>

namespace game::core {

Buff::Buff(BuffType type, double durationSeconds, double magnitude)
    : type_(type),
      remainingSeconds_(std::max(0.0, durationSeconds)),
      magnitude_(magnitude) {}

void Buff::tick(double deltaSeconds) {
    remainingSeconds_ = std::max(0.0, remainingSeconds_ - deltaSeconds);
}

} // namespace game::core
