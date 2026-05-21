#ifndef GAMEPROJECT_CORE_COMBAT_TARGETSELECTOR_H
#define GAMEPROJECT_CORE_COMBAT_TARGETSELECTOR_H

#include "core/base/Entity.h"
#include "core/map/Map.h"
#include <memory>
#include <vector>

namespace game::core {

// 索敌工具。
//
// 根据 owner 的位置、射程、优先级表和候选目标列表，选出最合适的目标。
// 攻击单位用它找敌人，治疗单位也用它找受伤友军。
class TargetSelector {
public:
    // requireDamagedAlly=true 时，只会选择未满血目标，适用于治疗。
    static std::shared_ptr<Entity> selectTarget(
        const Entity& owner,
        const std::vector<std::shared_ptr<Entity>>& candidates,
        const Map& map,
        int baseRange,
        const std::vector<ObjectType>& priorityList,
        bool requireDamagedAlly = false);
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_COMBAT_TARGETSELECTOR_H
