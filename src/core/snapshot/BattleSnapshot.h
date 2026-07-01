#ifndef GAMEPROJECT_CORE_SNAPSHOT_BATTLESNAPSHOT_H
#define GAMEPROJECT_CORE_SNAPSHOT_BATTLESNAPSHOT_H

#include "core/snapshot/MapSnapshot.h"
#include "core/snapshot/MonsterSnapshot.h"
#include "core/snapshot/ProjectileSnapshot.h"
#include "core/snapshot/UnitSnapshot.h"
#include <cstdint>
#include <vector>

namespace game::core {

enum class BattleEventType : std::uint8_t {
    Damage = 0,
    Heal,
    ResourceGain,
    MonsterKilled,
    MonsterEscaped,
    Deploy,
    Upgrade,
    Move,
    Recall
};

struct BattleEvent {
    int sequenceId = 0;
    BattleEventType type = BattleEventType::Damage;
    int sourceId = -1;
    int targetId = -1;
    int amount = 0;
    int row = -1;
    int col = -1;
    CardKind cardKind = CardKind::Attack;
    MonsterKind monsterKind = MonsterKind::AtkNormal;
};

// 一帧完整战斗状态。
//
// app/UI 层应读取 BattleSnapshot 来渲染画面和状态栏，
// 不直接遍历或修改 BattleManager 内部对象。
struct BattleSnapshot {
    // 顶部状态栏数据。
    int currentWave = 0;
    int resources = 0;
    int baseHealth = 0;
    int opponentResources = 0;
    int opponentBaseHealth = 0;
    int defeatedMonsters = 0;
    int escapedMonsters = 0;
    bool waveActive = false;
    bool gameOver = false;
    // 地图、卡牌和怪物展示数据。
    MapSnapshot map;
    std::vector<UnitSnapshot> units;
    std::vector<MonsterSnapshot> monsters;
    std::vector<ProjectileSnapshot> projectiles;
    std::vector<BattleEvent> events;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_SNAPSHOT_BATTLESNAPSHOT_H
