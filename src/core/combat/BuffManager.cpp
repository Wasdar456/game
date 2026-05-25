#include "core/combat/BuffManager.h"
#include <algorithm>

namespace game::core {

const std::vector<Buff> BuffManager::empty_;

void BuffManager::addBuff(int entityId, const Buff& buff) {
    buffs_[entityId].push_back(buff);
}

void BuffManager::update(double deltaSeconds) {
    for (auto it = buffs_.begin(); it != buffs_.end();) {
        auto& list = it->second;
        for (Buff& buff : list) buff.tick(deltaSeconds);
        list.erase(std::remove_if(list.begin(), list.end(),
                                  [](const Buff& buff) { return buff.expired(); }),
                   list.end());
        if (list.empty()) {
            it = buffs_.erase(it);
        } else {
            ++it;
        }
    }
}

void BuffManager::clear(int entityId) {
    buffs_.erase(entityId);
}

bool BuffManager::hasBuff(int entityId, BuffType type) const {
    auto it = buffs_.find(entityId);
    if (it == buffs_.end()) return false;
    return std::any_of(it->second.begin(), it->second.end(),
                       [type](const Buff& buff) { return buff.type() == type; });
}

double BuffManager::totalMagnitude(int entityId, BuffType type) const {
    auto it = buffs_.find(entityId);
    if (it == buffs_.end()) return 0.0;
    double total = 0.0;
    for (const Buff& buff : it->second) {
        if (buff.type() == type) total += buff.magnitude();
    }
    return total;
}

const std::vector<Buff>& BuffManager::buffsFor(int entityId) const {
    auto it = buffs_.find(entityId);
    return it == buffs_.end() ? empty_ : it->second;
}

} // namespace game::core
