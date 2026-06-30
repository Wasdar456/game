#ifndef GAMEPROJECT_CORE_SYSTEMS_WAVESPAWNER_H
#define GAMEPROJECT_CORE_SYSTEMS_WAVESPAWNER_H

#include "core/units/MonsterTypes.h"
#include <cstdint>
#include <memory>
#include <random>
#include <utility>
#include <vector>

namespace game::core {

// 单波怪物配置。后续 data_manager 读取 level_config.txt 后可直接生成此结构。
struct WaveConfig {
    int waveId = 1;
    MonsterKind kind = MonsterKind::AtkNormal;
    int count = 5;
    double intervalSeconds = 1.0;
    double healthMultiplier = 1.0;
    double attackMultiplier = 1.0;
};

struct ScheduledMonsterSpawn {
    std::shared_ptr<Monster> monster;
    double spawnTimeSeconds = 0.0;
};

// 波次生成器。
//
// 支持两种模式：
// 1. spawnWave(config)：按外部配置生成；
// 2. spawnDeterministicWave(waveId)：用同步 RNG 生成 PVP 可复现波次。
class WaveSpawner {
public:
    explicit WaveSpawner(int firstMonsterId = 1000);

    void setSeed(std::uint32_t seed);
    void setDifficulty(int difficulty);
    void setSpawnPoint(MapPosition spawnPoint) { spawnPoint_ = spawnPoint; }
    void setDefaultPath(std::vector<MapPosition> path);
    void setDefaultPaths(std::vector<std::vector<MapPosition>> paths);

    // 按指定配置生成怪物，并给每只怪设置默认路径。
    std::vector<std::shared_ptr<Monster>> spawnWave(const WaveConfig& config);

    // 按指定配置生成带出场时间的计划表。
    std::vector<ScheduledMonsterSpawn> scheduleWave(const WaveConfig& config);

    // 根据 waveId 和 rng_ 生成确定性波次，适合 PVP 双端同步。
    std::vector<std::shared_ptr<Monster>> spawnDeterministicWave(int waveId);
    std::vector<ScheduledMonsterSpawn> scheduleDeterministicWave(int waveId);

private:
    // 按波次阶段随机选择怪物类型。
    MonsterKind randomMonsterKind(int waveId, std::mt19937& rng) const;
    MonsterKind randomMonsterKindWithinBudget(int waveId, int remainingBudget, std::mt19937& rng) const;
    std::vector<MonsterKind> generateMonsterKindsForBudget(int waveId, int budget, std::mt19937& rng) const;
    int monsterBudgetCost(MonsterKind kind) const;
    int waveBudget(int waveId) const;
    WaveConfig deterministicConfig(int waveId) const;
    std::uint32_t waveSeed(int waveId) const;

    // 下一个怪物 id。
    int nextMonsterId_;
    // 怪物出生点。
    MapPosition spawnPoint_;
    // 当前关卡默认行走路径。
    std::vector<MapPosition> defaultPath_;
    // PVP 多核心路线。为空时退回 defaultPath_。
    std::vector<std::vector<MapPosition>> defaultPaths_;
    // 同步随机数引擎。网络层同步 seed 后应调用 setSeed。
    std::uint32_t seed_ = 0;
    std::mt19937 rng_;
    int difficulty_ = 0;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_SYSTEMS_WAVESPAWNER_H
