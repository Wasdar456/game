#include "core/systems/WaveSpawner.h"
#include <algorithm>

namespace game::core {

WaveSpawner::WaveSpawner(int firstMonsterId)
    : nextMonsterId_(firstMonsterId),
      spawnPoint_(0, 0),
      rng_(std::random_device{}()) {}

void WaveSpawner::setSeed(std::uint32_t seed) {
    rng_.seed(seed);
}

std::vector<std::shared_ptr<Monster>> WaveSpawner::spawnWave(const WaveConfig& config) {
    // 按配置一次性生成整波怪物。实际“按间隔出生”可由 app 层或未来队列扩展。
    std::vector<std::shared_ptr<Monster>> result;
    result.reserve(static_cast<std::size_t>(std::max(0, config.count)));

    for (int i = 0; i < config.count; ++i) {
        auto monster = createMonster(config.kind, nextMonsterId_++, spawnPoint_);
        if (!monster) continue;
        if (config.healthMultiplier != 1.0) {
            // 根据波次配置缩放生命值。
            int scaled = static_cast<int>(monster->maxHp() * config.healthMultiplier);
            monster->setMaxHp(std::max(1, scaled));
            monster->setHp(monster->maxHp());
        }
        // 每只怪物使用当前关卡默认路径。
        monster->setPath(defaultPath_);
        result.push_back(monster);
    }

    return result;
}

std::vector<std::shared_ptr<Monster>> WaveSpawner::spawnDeterministicWave(int waveId) {
    // PVP 模式下双方只要 seed 和 waveId 一致，就能生成同样的怪物序列。
    WaveConfig config;
    config.waveId = waveId;
    config.kind = randomMonsterKind(waveId);
    config.count = 3 + waveId;
    config.healthMultiplier = 1.0 + waveId * 0.08;
    return spawnWave(config);
}

MonsterKind WaveSpawner::randomMonsterKind(int waveId) {
    // 前期主要生成资源怪和基础攻击怪，后期开放更多攻击怪。
    std::uniform_int_distribution<int> early(0, 5);
    std::uniform_int_distribution<int> late(3, 9);
    int value = waveId < 4 ? early(rng_) : late(rng_);
    return static_cast<MonsterKind>(value);
}

} // namespace game::core
