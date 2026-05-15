#ifndef GAMEPROJECT_CORE_SYSTEMS_BATTLEMANAGER_H
#define GAMEPROJECT_CORE_SYSTEMS_BATTLEMANAGER_H

#include "core/map/Map.h"
#include "core/snapshot/BattleSnapshot.h"
#include "core/systems/CardSystem.h"
#include "core/systems/ResourceManager.h"
#include "core/systems/SkillSystem.h"
#include "core/systems/WaveSpawner.h"
#include <memory>
#include <vector>

namespace game::core {

// 战斗总管理器。
//
// BattleManager 是 core 层对 app/UI 暴露的主要入口。
// 它组合地图、资源、卡牌系统、技能系统、波次生成器和怪物列表，
// 负责推进一局战斗并产出只读快照。
class BattleManager {
public:
    BattleManager();

    Map& map() { return map_; }
    const Map& map() const { return map_; }
    ResourceManager& resources() { return resources_; }
    const ResourceManager& resources() const { return resources_; }
    CardSystem& cardSystem() { return cardSystem_; }
    const std::vector<std::shared_ptr<Monster>>& monsters() const { return monsters_; }

    // 关卡初始化接口。
    void setSpawnPoint(MapPosition spawnPoint);
    void setPath(std::vector<MapPosition> path);

    // PVP 模式下由网络层同步 seed 后调用。
    void setRandomSeed(std::uint32_t seed);

    // 玩家操作入口：部署、升级、移动、撤回。
    std::shared_ptr<Card> deployCard(CardKind kind, MapPosition position);
    bool upgradeCard(int unitId);
    bool moveCard(int unitId, MapPosition target);
    bool recallCard(int unitId);

    // 波次入口。startWave 使用确定性波次，spawnWave 使用显式配置。
    void startWave(int waveId);
    void spawnWave(const WaveConfig& config);

    // 推进战斗一帧：自动技能、怪物移动、死亡/逃逸清理。
    void update(double deltaSeconds);

    // 清空场上单位和波次状态。
    void clearBattle();

    int currentWave() const { return currentWave_; }
    bool gameOver() const { return resources_.baseDestroyed(); }

    // 给 UI/app 的只读状态快照。
    BattleSnapshot snapshot() const;

private:
    // 移除死亡怪物并发放奖励；移除逃逸怪物并扣基地血。
    void removeResolvedMonsters();

    // 将地图转换成 UI 可读快照。
    MapSnapshot makeMapSnapshot() const;

    // 关卡地图。
    Map map_;
    // 资源和基地状态。
    ResourceManager resources_;
    // 玩家卡牌管理。
    CardSystem cardSystem_;
    // 自动技能处理。
    SkillSystem skillSystem_;
    // 波次生成器。
    WaveSpawner waveSpawner_;
    // 当前存活怪物。
    std::vector<std::shared_ptr<Monster>> monsters_;
    // 当前波次编号。
    int currentWave_;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_SYSTEMS_BATTLEMANAGER_H
