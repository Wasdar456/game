#ifndef GAMEPROJECT_CORE_SNAPSHOT_UNITSNAPSHOT_H
#define GAMEPROJECT_CORE_SNAPSHOT_UNITSNAPSHOT_H

#include "core/base/CoreTypes.h"
#include "core/map/MapPosition.h"

namespace game::core {

// UI 显示用卡牌快照。
//
// Snapshot 只保存值，不暴露指针，避免 UI 层误改 core 状态。
struct UnitSnapshot {
    // 单位 id，与 Card::id() 对应。
    int id = -1;
    // 单位类型，决定 UI 图标/贴图。
    ObjectType type = ObjectType::None;
    // 网格位置。
    int row = 0;
    int col = 0;
    // 战斗数值。
    int hp = 0;
    int maxHp = 0;
    int attack = 0;
    // 卡牌展示信息。
    int level = 1;
    int range = 0;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_SNAPSHOT_UNITSNAPSHOT_H
