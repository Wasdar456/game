#ifndef GAMEPROJECT_CORE_SNAPSHOT_MONSTERSNAPSHOT_H
#define GAMEPROJECT_CORE_SNAPSHOT_MONSTERSNAPSHOT_H

#include "core/base/CoreTypes.h"

namespace game::core {

// UI 显示用怪物快照。
struct MonsterSnapshot {
    // 怪物 id。
    int id = -1;
    // 怪物种类，决定 UI 贴图和说明。
    MonsterKind kind = MonsterKind::AtkNormal;
    // 网格位置。
    int row = 0;
    int col = 0;
    // 生命值。
    int hp = 0;
    int maxHp = 0;
    // 是否已经到达终点。通常 BattleManager 会及时移除，保留此字段便于调试。
    bool escaped = false;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_SNAPSHOT_MONSTERSNAPSHOT_H
