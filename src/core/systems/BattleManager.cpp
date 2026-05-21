#include "core/systems/BattleManager.h"
#include <algorithm>
#include <utility>

namespace game::core {

BattleManager::BattleManager()
    : map_(),
      resources_(),
      cardSystem_(1),
      waveSpawner_(1000),
      currentWave_(0) {}

void BattleManager::setSpawnPoint(MapPosition spawnPoint) {
    waveSpawner_.setSpawnPoint(spawnPoint);
}

void BattleManager::setPath(std::vector<MapPosition> path) {
    waveSpawner_.setDefaultPath(std::move(path));
}

void BattleManager::setRandomSeed(std::uint32_t seed) {
    waveSpawner_.setSeed(seed);
}

std::shared_ptr<Card> BattleManager::deployCard(CardKind kind, MapPosition position) {
    return cardSystem_.deploy(kind, position, map_, resources_);
}

bool BattleManager::upgradeCard(int unitId) {
    return cardSystem_.upgrade(unitId, resources_);
}

bool BattleManager::moveCard(int unitId, MapPosition target) {
    return cardSystem_.move(unitId, target, map_, resources_);
}

bool BattleManager::recallCard(int unitId) {
    return cardSystem_.recall(unitId, map_, resources_);
}

void BattleManager::startWave(int waveId) {
    // 默认波次使用确定性生成，适合 PVP 网络同步。
    currentWave_ = waveId;
    auto spawned = waveSpawner_.spawnDeterministicWave(waveId);
    monsters_.insert(monsters_.end(), spawned.begin(), spawned.end());
}

void BattleManager::spawnWave(const WaveConfig& config) {
    // 显式配置波次适合 PVE 或 data_manager 读取关卡文件后的结果。
    currentWave_ = config.waveId;
    auto spawned = waveSpawner_.spawnWave(config);
    monsters_.insert(monsters_.end(), spawned.begin(), spawned.end());
}

void BattleManager::update(double deltaSeconds) {
    // 基地被摧毁后停止推进，避免继续刷奖励或扣血。
    if (gameOver()) return;

    // 先处理卡牌自动技能，再移动怪物。这样本帧已经死亡的怪不会继续前进。
    skillSystem_.update(deltaSeconds, cardSystem_.cards(), monsters_, map_, resources_);

    for (auto& monster : monsters_) {
        if (monster && !monster->isDead()) {
            monster->update(deltaSeconds);
        }
    }

    // 最后统一结算死亡奖励和逃逸扣血。
    removeResolvedMonsters();
}

void BattleManager::clearBattle() {
    cardSystem_.clear(map_);
    monsters_.clear();
    currentWave_ = 0;
}

BattleSnapshot BattleManager::snapshot() const {
    // 快照只复制值，不暴露内部对象，UI 读取安全。
    BattleSnapshot result;
    result.currentWave = currentWave_;
    result.resources = resources_.resources();
    result.baseHealth = resources_.baseHealth();
    result.gameOver = gameOver();
    result.map = makeMapSnapshot();

    for (const auto& card : cardSystem_.cards()) {
        // 逐个卡牌转成 UI 显示结构。
        if (!card) continue;
        UnitSnapshot unit;
        unit.id = card->id();
        unit.type = card->type();
        unit.row = card->row();
        unit.col = card->col();
        unit.hp = card->hp();
        unit.maxHp = card->maxHp();
        unit.attack = card->attack();
        unit.level = card->level();
        unit.range = card->attackRange();
        unit.moveLimit = card->moveLimit();
        result.units.push_back(unit);
    }

    for (const auto& monster : monsters_) {
        // 逐个怪物转成 UI 显示结构。
        if (!monster) continue;
        MonsterSnapshot snap;
        snap.id = monster->id();
        snap.kind = monster->kind();
        snap.row = monster->row();
        snap.col = monster->col();
        snap.hp = monster->hp();
        snap.maxHp = monster->maxHp();
        snap.escaped = monster->escaped();
        result.monsters.push_back(snap);
    }

    return result;
}

void BattleManager::removeResolvedMonsters() {
    // 使用 erase-remove 一次性移除所有死亡或逃逸怪物。
    monsters_.erase(std::remove_if(monsters_.begin(), monsters_.end(),
        [this](const std::shared_ptr<Monster>& monster) {
            if (!monster) return true;
            if (monster->isDead()) {
                // 死亡怪物发放资源奖励。
                monster->onDeath(resources_);
                return true;
            }
            if (monster->escaped()) {
                // 逃逸怪物伤害基地。
                resources_.damageBase(monster->coreDamage());
                return true;
            }
            return false;
        }),
        monsters_.end());
}

MapSnapshot BattleManager::makeMapSnapshot() const {
    // 完整复制地图格状态，便于 UI 直接绘制地形和部署占用。
    MapSnapshot snap;
    snap.rows = map_.rows();
    snap.cols = map_.cols();
    snap.grids.reserve(static_cast<std::size_t>(snap.rows * snap.cols));

    for (int row = 0; row < map_.rows(); ++row) {
        for (int col = 0; col < map_.cols(); ++col) {
            const MapGrid* grid = map_.gridAt({row, col});
            if (!grid) continue;
            GridSnapshot cell;
            cell.row = row;
            cell.col = col;
            cell.terrain = grid->terrainType();
            cell.height = grid->height();
            cell.occupied = grid->isOccupied();
            cell.occupantId = grid->occupantId();
            snap.grids.push_back(cell);
        }
    }
    return snap;
}

} // namespace game::core
