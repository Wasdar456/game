#include "core/systems/WaveSpawner.h"
#include "core/base/DebugConfig.h"
#include <QDebug>
#include <algorithm>

namespace game::core {

WaveSpawner::WaveSpawner(int firstMonsterId)
    : nextMonsterId_(firstMonsterId),
      spawnPoint_(0, 0),
      rng_(std::random_device{}()) {}

void WaveSpawner::setSeed(std::uint32_t seed) {
    seed_ = seed;
    rng_.seed(seed);
}

void WaveSpawner::setDefaultPath(std::vector<MapPosition> path) {
    defaultPaths_.clear();
    defaultPath_ = std::move(path);
    if (!defaultPath_.empty()) {
        spawnPoint_ = defaultPath_.front();
    }
}

void WaveSpawner::setDefaultPaths(std::vector<std::vector<MapPosition>> paths) {
    defaultPaths_.clear();
    for (auto& path : paths) {
        if (!path.empty()) {
            defaultPaths_.push_back(std::move(path));
        }
    }
    if (!defaultPaths_.empty()) {
        defaultPath_ = defaultPaths_.front();
        spawnPoint_ = defaultPaths_.front().front();
    }
}

std::vector<std::shared_ptr<Monster>> WaveSpawner::spawnWave(const WaveConfig& config) {
    std::vector<std::shared_ptr<Monster>> result;
    auto schedule = scheduleWave(config);
    result.reserve(schedule.size());
    for (auto& item : schedule) {
        if (item.monster) result.push_back(std::move(item.monster));
    }
    return result;
}

std::vector<ScheduledMonsterSpawn> WaveSpawner::scheduleWave(const WaveConfig& config) {
    std::vector<ScheduledMonsterSpawn> result;
    const bool useMultiplePaths = !defaultPaths_.empty();
    const std::size_t pathCount = useMultiplePaths ? defaultPaths_.size() : 1;
    int totalCount = std::max(0, config.count);
    if (useMultiplePaths && totalCount > 0) {
        totalCount = std::max(totalCount, static_cast<int>(pathCount));
    }
    result.reserve(static_cast<std::size_t>(totalCount));

    for (int i = 0; i < totalCount; ++i) {
        const auto& path = useMultiplePaths
                               ? defaultPaths_[static_cast<std::size_t>(i) % pathCount]
                               : defaultPath_;
        const MapPosition spawnPoint = !path.empty() ? path.front() : spawnPoint_;

        auto monster = createMonster(config.kind, nextMonsterId_++, spawnPoint);
        if (!monster) continue;
        if (config.healthMultiplier != 1.0) {
            // 根据波次配置缩放生命值。
            int scaled = static_cast<int>(monster->maxHp() * config.healthMultiplier);
            monster->setMaxHp(std::max(1, scaled));
            monster->setHp(monster->maxHp());
        }
        // PVP 使用多路线轮流分配，避免所有怪物只压一方核心。
        monster->setPath(path);
        const double spawnTime = (pathCount > 0)
                                     ? (i / static_cast<int>(pathCount)) * config.intervalSeconds
                                     : i * config.intervalSeconds;
        if (DebugConfig::DEBUG_ENABLED) {
            const auto end = path.empty() ? spawnPoint : path.back();
            qDebug() << "[WaveSpawner] spawn monster" << monster->id()
                     << "kind" << static_cast<int>(monster->kind())
                     << "hp" << monster->hp()
                     << "time" << spawnTime
                     << "from (" << spawnPoint.row << "," << spawnPoint.col << ")"
                     << "to (" << end.row << "," << end.col << ")";
        }
        result.push_back({monster, spawnTime});
    }

    return result;
}

std::vector<std::shared_ptr<Monster>> WaveSpawner::spawnDeterministicWave(int waveId) {
    auto schedule = scheduleDeterministicWave(waveId);
    std::vector<std::shared_ptr<Monster>> result;
    result.reserve(schedule.size());
    for (auto& item : schedule) {
        if (item.monster) result.push_back(std::move(item.monster));
    }
    return result;
}

std::vector<ScheduledMonsterSpawn> WaveSpawner::scheduleDeterministicWave(int waveId) {
    WaveConfig config = deterministicConfig(waveId);
    const bool useMultiplePaths = !defaultPaths_.empty();
    const std::size_t pathCount = useMultiplePaths ? defaultPaths_.size() : 1;

    std::mt19937 waveRng(waveSeed(waveId));
    const int budget = waveBudget(waveId);
    std::vector<MonsterKind> kinds = generateMonsterKindsForBudget(waveId, budget, waveRng);
    if (useMultiplePaths && !kinds.empty()) {
        while (kinds.size() < pathCount) {
            kinds.push_back(MonsterKind::AtkFast);
        }
    }

    const int totalCount = static_cast<int>(kinds.size());
    const int routeOffset = useMultiplePaths
                                ? std::uniform_int_distribution<int>(0, static_cast<int>(pathCount) - 1)(waveRng)
                                : 0;

    std::vector<ScheduledMonsterSpawn> result;
    result.reserve(static_cast<std::size_t>(totalCount));

    for (int i = 0; i < totalCount; ++i) {
        const MonsterKind kind = kinds[static_cast<std::size_t>(i)];
        const int routeIndex = useMultiplePaths
                                   ? (i + routeOffset) % static_cast<int>(pathCount)
                                   : 0;
        const auto& path = useMultiplePaths ? defaultPaths_[static_cast<std::size_t>(routeIndex)] : defaultPath_;
        const MapPosition spawnPoint = !path.empty() ? path.front() : spawnPoint_;
        const int monsterId = 1000 + waveId * 100 + i;
        auto monster = createMonster(kind, monsterId, spawnPoint);
        if (!monster) continue;

        if (config.healthMultiplier != 1.0) {
            int scaled = static_cast<int>(monster->maxHp() * config.healthMultiplier);
            monster->setMaxHp(std::max(1, scaled));
            monster->setHp(monster->maxHp());
        }
        monster->setPath(path);

        const double spawnTime = (pathCount > 0)
                                     ? (i / static_cast<int>(pathCount)) * config.intervalSeconds
                                     : i * config.intervalSeconds;
        if (DebugConfig::DEBUG_ENABLED) {
            const auto end = path.empty() ? spawnPoint : path.back();
            qDebug() << "[WaveSpawner] deterministic monster" << monster->id()
                     << "wave" << waveId
                     << "budget" << budget
                     << "kind" << static_cast<int>(monster->kind())
                     << "cost" << monsterBudgetCost(kind)
                     << "hp" << monster->hp()
                     << "time" << spawnTime
                     << "route" << routeIndex
                     << "from (" << spawnPoint.row << "," << spawnPoint.col << ")"
                     << "to (" << end.row << "," << end.col << ")";
        }
        result.push_back({monster, spawnTime});
    }

    return result;
}

WaveConfig WaveSpawner::deterministicConfig(int waveId) const {
    WaveConfig config;
    config.waveId = waveId;
    if (DebugConfig::DEBUG_ENABLED && DebugConfig::MONSTERS_PER_WAVE > 0) {
        config.count = DebugConfig::MONSTERS_PER_WAVE;
    } else {
        config.count = 0;
    }
    config.intervalSeconds = 0.9;
    config.healthMultiplier = 1.0 + waveId * 0.08;
    return config;
}

std::uint32_t WaveSpawner::waveSeed(int waveId) const {
    std::uint32_t value = seed_ ^ 0x9E3779B9u;
    value ^= static_cast<std::uint32_t>(waveId) * 0x85EBCA6Bu;
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    value ^= value >> 16;
    return value;
}

MonsterKind WaveSpawner::randomMonsterKind(int waveId, std::mt19937& rng) const {
    // 战斗波次先生成攻击怪，避免低血资源怪进入主路线后被塔瞬间秒杀。
    std::uniform_int_distribution<int> early(3, 5);
    std::uniform_int_distribution<int> late(3, 9);
    int value = waveId < 4 ? early(rng) : late(rng);
    return static_cast<MonsterKind>(value);
}

MonsterKind WaveSpawner::randomMonsterKindWithinBudget(int waveId, int remainingBudget, std::mt19937& rng) const {
    std::vector<MonsterKind> candidates;
    candidates.push_back(MonsterKind::AtkFast);
    candidates.push_back(MonsterKind::AtkNormal);
    if (waveId >= 2) candidates.push_back(MonsterKind::AtkSapper);
    if (waveId >= 3) candidates.push_back(MonsterKind::AtkTank);
    if (waveId >= 4) candidates.push_back(MonsterKind::AtkRanged);
    if (waveId >= 5) candidates.push_back(MonsterKind::AtkBerserk);
    if (waveId >= 6) candidates.push_back(MonsterKind::AtkRegen);

    candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                                    [this, remainingBudget](MonsterKind kind) {
                                        return monsterBudgetCost(kind) > remainingBudget;
                                    }),
                     candidates.end());
    if (candidates.empty()) return MonsterKind::AtkFast;

    std::vector<int> weights;
    weights.reserve(candidates.size());
    for (MonsterKind kind : candidates) {
        int weight = 12;
        switch (kind) {
            case MonsterKind::AtkFast: weight = 16; break;
            case MonsterKind::AtkNormal: weight = 18; break;
            case MonsterKind::AtkSapper: weight = 10; break;
            case MonsterKind::AtkTank: weight = 7 + waveId; break;
            case MonsterKind::AtkRanged: weight = 8; break;
            case MonsterKind::AtkBerserk: weight = 7; break;
            case MonsterKind::AtkRegen: weight = 6; break;
            default: weight = 1; break;
        }
        weights.push_back(weight);
    }

    std::discrete_distribution<int> distribution(weights.begin(), weights.end());
    return candidates[static_cast<std::size_t>(distribution(rng))];
}

std::vector<MonsterKind> WaveSpawner::generateMonsterKindsForBudget(int waveId, int budget, std::mt19937& rng) const {
    std::vector<MonsterKind> kinds;
    int remaining = std::max(0, budget);
    const int minimumCost = monsterBudgetCost(MonsterKind::AtkFast);

    while (remaining >= minimumCost) {
        MonsterKind kind = randomMonsterKindWithinBudget(waveId, remaining, rng);
        int cost = monsterBudgetCost(kind);
        if (cost > remaining) break;
        kinds.push_back(kind);
        remaining -= cost;
    }

    return kinds;
}

int WaveSpawner::monsterBudgetCost(MonsterKind kind) const {
    switch (kind) {
        case MonsterKind::AtkFast: return 2;
        case MonsterKind::AtkNormal: return 3;
        case MonsterKind::AtkSapper: return 4;
        case MonsterKind::AtkRanged: return 5;
        case MonsterKind::AtkBerserk: return 5;
        case MonsterKind::AtkRegen: return 6;
        case MonsterKind::AtkTank: return 8;
        case MonsterKind::ResBasic: return 2;
        case MonsterKind::ResFast: return 2;
        case MonsterKind::ResTank: return 6;
    }
    return 3;
}

int WaveSpawner::waveBudget(int waveId) const {
    const int safeWave = std::max(1, waveId);
    return 8 + safeWave * 5;
}

} // namespace game::core
