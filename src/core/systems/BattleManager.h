#ifndef GAMEPROJECT_CORE_SYSTEMS_BATTLEMANAGER_H
#define GAMEPROJECT_CORE_SYSTEMS_BATTLEMANAGER_H

#include "core/map/Map.h"
#include "core/combat/Projectile.h"
#include "core/snapshot/BattleSnapshot.h"
#include "core/systems/CardSystem.h"
#include "core/systems/ResourceManager.h"
#include "core/systems/SkillSystem.h"
#include "core/systems/WaveSpawner.h"
#include "core/systems/VisionManager.h"
#include <memory>
#include <unordered_map>
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

    // PVP 模式：对方的资源和卡牌系统
    ResourceManager& opponentResources() { return opponentResources_; }
    CardSystem& opponentCardSystem() { return opponentCardSystem_; }
    VisionManager& visionManager() { return visionManager_; }

    // 关卡初始化接口。
    void setSpawnPoint(MapPosition spawnPoint);
    void setPath(std::vector<MapPosition> path);
    void setPaths(std::vector<std::vector<MapPosition>> paths);

    // PVP 模式下由网络层同步 seed 后调用。
    void setRandomSeed(std::uint32_t seed);
    void setPveDifficulty(int difficulty);

    // 设置 PVP 模式
    void setPvpMode(bool isPvp) { isPvp_ = isPvp; }

    // 玩家操作入口：部署、升级、移动、撤回。
    std::shared_ptr<Card> deployCard(CardKind kind, MapPosition position);
    bool upgradeCard(int unitId);
    bool moveCard(int unitId, MapPosition target);
    bool recallCard(int unitId);

    // 对方操作入口（PVP 模式）
    std::shared_ptr<Card> deployOpponentCard(CardKind kind, MapPosition position);
    bool upgradeOpponentCard(int unitId);
    bool moveOpponentCard(int unitId, MapPosition target);
    bool recallOpponentCard(int unitId);

    // 波次入口。startWave 使用确定性波次，spawnWave 使用显式配置。
    void startWave(int waveId);
    void spawnWave(const WaveConfig& config);

    // 推进战斗一帧：自动技能、怪物移动、死亡/逃逸清理。
    void update(double deltaSeconds);

    // 清空场上单位和波次状态。
    void clearBattle();
    void clearMonsters();
    void syncPvpUnitsFromHostSnapshot(const BattleSnapshot& hostSnapshot, bool localIsHost);
    void rebuildMapOccupancy();

    int currentWave() const { return currentWave_; }
    int defeatedMonsters() const { return defeatedMonsters_; }
    int escapedMonsters() const { return escapedMonsters_; }
    bool waveActive() const { return !monsters_.empty() || !pendingSpawns_.empty(); }
    bool gameOver() const { return resources_.baseDestroyed() || opponentResources_.baseDestroyed(); }

    // 给 UI/app 的只读状态快照。
    BattleSnapshot snapshot() const;
    // 获取对方视角的快照
    BattleSnapshot opponentSnapshot() const;

private:
    void recordEvent(BattleEventType type,
                     int sourceId,
                     int targetId,
                     int amount,
                     MapPosition position,
                     CardKind cardKind = CardKind::Attack,
                     MonsterKind monsterKind = MonsterKind::AtkNormal);
    // 移除死亡怪物并发放奖励；移除逃逸怪物并扣基地血。
    void removeResolvedMonsters();
    void releaseDueSpawns();
    void updateProjectiles(double deltaSeconds);
    void updateMonsterAttacks(double deltaSeconds);
    void applyProjectileHit(const Projectile& projectile);
    std::shared_ptr<Entity> findMonsterTarget(const std::shared_ptr<Monster>& monster) const;

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
    std::vector<ScheduledMonsterSpawn> pendingSpawns_;
    std::vector<Projectile> projectiles_;
    std::unordered_map<int, double> monsterCooldowns_;
    std::vector<BattleEvent> events_;
    int nextEventSequence_ = 1;
    double waveElapsedSeconds_ = 0.0;
    // 当前波次编号。
    int currentWave_;
    int defeatedMonsters_ = 0;
    int escapedMonsters_ = 0;

    // PVP 模式相关
    bool isPvp_ = false;
    ResourceManager opponentResources_;
    CardSystem opponentCardSystem_;
    VisionManager visionManager_;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_SYSTEMS_BATTLEMANAGER_H
