#include "core/combat/TargetSelector.h"
#include "core/base/Constants.h"
#include <algorithm>

namespace game::core {

std::shared_ptr<Entity> TargetSelector::selectTarget(
    const Entity& owner,
    const std::vector<std::shared_ptr<Entity>>& candidates,
    const Map& map,
    int baseRange,
    const std::vector<ObjectType>& priorityList,
    bool requireDamagedAlly) {

    int effectiveRange = baseRange;
    const MapGrid* ownerGrid = map.gridAt(owner.position());
    if (ownerGrid && ownerGrid->height() > 0) {
        // 高台单位获得额外射程，和 README 中的地形优势一致。
        effectiveRange += constants::HighGroundRangeBonus;
    }

    std::vector<std::shared_ptr<Entity>> valid;
    for (const auto& candidate : candidates) {
        // 过滤空指针、死亡实体和自己。
        if (!candidate || candidate->isDead() || candidate->id() == owner.id()) continue;
        // 过滤射程外目标。
        if (owner.position().manhattanDistanceTo(candidate->position()) > effectiveRange) continue;
        // 治疗模式下只考虑未满血友军。
        if (requireDamagedAlly && candidate->hp() >= candidate->maxHp()) continue;
        valid.push_back(candidate);
    }

    if (valid.empty()) return nullptr;

    std::sort(valid.begin(), valid.end(), [&](const auto& a, const auto& b) {
        // 第一优先级：单位自己的 priorityList。
        auto ia = std::find(priorityList.begin(), priorityList.end(), a->type());
        auto ib = std::find(priorityList.begin(), priorityList.end(), b->type());
        if (ia != ib) {
            if (ia != priorityList.end() && ib != priorityList.end()) return ia < ib;
            return ia != priorityList.end();
        }

        // 治疗时优先血量百分比最低的目标。
        if (requireDamagedAlly && a->getHpPercent() != b->getHpPercent()) {
            return a->getHpPercent() < b->getHpPercent();
        }

        // 然后比较距离，距离近者优先。
        int da = owner.position().manhattanDistanceTo(a->position());
        int db = owner.position().manhattanDistanceTo(b->position());
        if (da != db) return da < db;

        // 再比较当前血量，低血目标优先。
        if (a->hp() != b->hp()) return a->hp() < b->hp();

        // 最后用 id 兜底，保证双方模拟排序稳定一致。
        return a->id() < b->id();
    });

    return valid.front();
}

} // namespace game::core
