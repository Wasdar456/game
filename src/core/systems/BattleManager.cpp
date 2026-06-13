#include "core/systems/BattleManager.h"
#include <QDebug>
#include <algorithm>
#include <set>
#include <utility>

namespace game::core {

BattleManager::BattleManager()
    : map_(),
      resources_(),
      cardSystem_(1),
      waveSpawner_(1000),
      waveElapsedSeconds_(0.0),
      currentWave_(0) {}

void BattleManager::setSpawnPoint(MapPosition spawnPoint) {
    waveSpawner_.setSpawnPoint(spawnPoint);
}

void BattleManager::setPath(std::vector<MapPosition> path) {
    waveSpawner_.setDefaultPath(std::move(path));
}

void BattleManager::setPaths(std::vector<std::vector<MapPosition>> paths) {
    waveSpawner_.setDefaultPaths(std::move(paths));
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

std::shared_ptr<Card> BattleManager::deployOpponentCard(CardKind kind, MapPosition position) {
    return opponentCardSystem_.deploy(kind, position, map_, opponentResources_);
}

bool BattleManager::upgradeOpponentCard(int unitId) {
    return opponentCardSystem_.upgrade(unitId, opponentResources_);
}

bool BattleManager::moveOpponentCard(int unitId, MapPosition target) {
    return opponentCardSystem_.move(unitId, target, map_, opponentResources_);
}

bool BattleManager::recallOpponentCard(int unitId) {
    return opponentCardSystem_.recall(unitId, map_, opponentResources_);
}

void BattleManager::startWave(int waveId) {
    // 默认波次使用确定性生成，适合 PVP 网络同步。
    currentWave_ = waveId;
    waveElapsedSeconds_ = 0.0;
    pendingSpawns_ = waveSpawner_.scheduleDeterministicWave(waveId);
    releaseDueSpawns();
}

void BattleManager::spawnWave(const WaveConfig& config) {
    // 显式配置波次适合 PVE 或 data_manager 读取关卡文件后的结果。
    currentWave_ = config.waveId;
    waveElapsedSeconds_ = 0.0;
    pendingSpawns_ = waveSpawner_.scheduleWave(config);
    releaseDueSpawns();
}

void BattleManager::update(double deltaSeconds) {
    // 基地被摧毁后停止推进，避免继续刷奖励或扣血。
    if (gameOver()) return;

    waveElapsedSeconds_ += deltaSeconds;
    releaseDueSpawns();
    updateProjectiles(deltaSeconds);

    if (isPvp_) {
        // PVP 模式：双方单位互相攻击
        skillSystem_.updatePvp(deltaSeconds,
                               cardSystem_.cards(),
                               opponentCardSystem_.cards(),
                               monsters_,
                               map_,
                               resources_,
                               opponentResources_,
                               projectiles_);
        cardSystem_.removeDead(map_);
        opponentCardSystem_.removeDead(map_);
    } else {
        // 单机模式：只攻击怪物
        skillSystem_.update(deltaSeconds, cardSystem_.cards(), monsters_, map_, resources_,
                            projectiles_);
    }

    updateMonsterAttacks(deltaSeconds);

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
    opponentCardSystem_.clear(map_);
    monsters_.clear();
    pendingSpawns_.clear();
    projectiles_.clear();
    monsterCooldowns_.clear();
    waveElapsedSeconds_ = 0.0;
    currentWave_ = 0;
    defeatedMonsters_ = 0;
    escapedMonsters_ = 0;
    isPvp_ = false;
    resources_.setResources(constants::InitialResources);
    resources_.setBaseHealth(constants::InitialBaseHealth);
    opponentResources_.setResources(constants::InitialResources);
    opponentResources_.setBaseHealth(constants::InitialBaseHealth);
}

void BattleManager::clearMonsters() {
    monsters_.clear();
    pendingSpawns_.clear();
    projectiles_.clear();
    monsterCooldowns_.clear();
    waveElapsedSeconds_ = 0.0;
}

void BattleManager::syncPvpUnitsFromHostSnapshot(const BattleSnapshot& hostSnapshot, bool localIsHost) {
    auto syncSide = [this, &hostSnapshot](CardSystem& system, bool hostSide) {
        std::set<int> seen;

        for (const auto& unit : hostSnapshot.units) {
            const bool unitIsHostSide = unit.id < 1000;
            if (unitIsHostSide != hostSide) continue;

            const int localId = unitIsHostSide ? unit.id : unit.id - 1000;
            auto card = system.findCard(localId);
            if (!card) continue;

            map_.clearOccupant(card->position());
            card->setPosition({unit.row, unit.col});
            card->setMaxHp(unit.maxHp);
            card->setHp(unit.hp);
            if (!card->isDead()) {
                map_.setOccupied(card->position(), true, card->id());
            }
            seen.insert(localId);
        }

        for (auto& card : system.cards()) {
            if (card && seen.find(card->id()) == seen.end()) {
                card->setHp(0);
            }
        }
        system.removeDead(map_);
    };

    if (localIsHost) {
        syncSide(cardSystem_, true);
        syncSide(opponentCardSystem_, false);
    } else {
        syncSide(cardSystem_, false);
        syncSide(opponentCardSystem_, true);
    }

    if (hostSnapshot.monsters.empty()) {
        clearMonsters();
    }
}

BattleSnapshot BattleManager::snapshot() const {
    // 快照只复制值，不暴露内部对象，UI 读取安全。
    BattleSnapshot result;
    result.currentWave = currentWave_;
    result.resources = resources_.resources();
    result.baseHealth = resources_.baseHealth();
    result.opponentResources = opponentResources_.resources();
    result.opponentBaseHealth = opponentResources_.baseHealth();
    result.defeatedMonsters = defeatedMonsters_;
    result.escapedMonsters = escapedMonsters_;
    result.waveActive = waveActive();
    result.gameOver = gameOver();
    result.map = makeMapSnapshot();

    // 己方单位
    for (const auto& card : cardSystem_.cards()) {
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

    // PVP 模式：对方单位（ID 偏移 1000 以区分）
    if (isPvp_) {
        for (const auto& card : opponentCardSystem_.cards()) {
            if (!card) continue;
            UnitSnapshot unit;
            unit.id = card->id() + 1000;  // 对方单位 ID + 1000
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
    }

    // 怪物
    for (const auto& monster : monsters_) {
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

    for (const auto& projectile : projectiles_) {
        ProjectileSnapshot snap;
        snap.sourceId = projectile.sourceId();
        snap.targetId = projectile.targetId();
        snap.fromRow = projectile.from().row;
        snap.fromCol = projectile.from().col;
        snap.toRow = projectile.to().row;
        snap.toCol = projectile.to().col;
        snap.progress = projectile.progress();
        snap.kind = projectile.kind();
        snap.splashRadius = projectile.splashRadius();
        result.projectiles.push_back(snap);
    }

    return result;
}

BattleSnapshot BattleManager::opponentSnapshot() const {
    // 对方视角的快照
    BattleSnapshot result;
    result.currentWave = currentWave_;
    result.resources = opponentResources_.resources();
    result.baseHealth = opponentResources_.baseHealth();
    result.opponentResources = resources_.resources();
    result.opponentBaseHealth = resources_.baseHealth();
    result.defeatedMonsters = defeatedMonsters_;
    result.escapedMonsters = escapedMonsters_;
    result.waveActive = waveActive();
    result.gameOver = gameOver();
    result.map = makeMapSnapshot();

    // 对方单位（从对方视角是己方）
    for (const auto& card : opponentCardSystem_.cards()) {
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

    // 己方单位（从对方视角是敌人，ID + 1000）
    for (const auto& card : cardSystem_.cards()) {
        if (!card) continue;
        UnitSnapshot unit;
        unit.id = card->id() + 1000;
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

    // 怪物
    for (const auto& monster : monsters_) {
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

    for (const auto& projectile : projectiles_) {
        ProjectileSnapshot snap;
        snap.sourceId = projectile.sourceId();
        snap.targetId = projectile.targetId();
        snap.fromRow = projectile.from().row;
        snap.fromCol = projectile.from().col;
        snap.toRow = projectile.to().row;
        snap.toCol = projectile.to().col;
        snap.progress = projectile.progress();
        snap.kind = projectile.kind();
        snap.splashRadius = projectile.splashRadius();
        result.projectiles.push_back(snap);
    }

    return result;
}

void BattleManager::updateProjectiles(double deltaSeconds) {
    for (auto& projectile : projectiles_) {
        projectile.update(deltaSeconds);
        if (projectile.reached()) {
            applyProjectileHit(projectile);
        }
    }

    projectiles_.erase(std::remove_if(projectiles_.begin(), projectiles_.end(),
        [](const Projectile& projectile) {
            return projectile.reached();
        }),
        projectiles_.end());
}

void BattleManager::applyProjectileHit(const Projectile& projectile) {
    auto target = projectile.target();
    if (!target || target->isDead()) return;

    const int splashRadius = projectile.splashRadius();
    if (splashRadius <= 0) {
        target->takeDamage(projectile.damage());
        return;
    }

    const MapPosition center = target->position();
    auto hitIfNear = [&](const std::shared_ptr<Entity>& entity) {
        if (!entity || entity->isDead()) return;
        if (center.manhattanDistanceTo(entity->position()) <= splashRadius) {
            entity->takeDamage(projectile.damage());
        }
    };

    if (projectile.owner() == ProjectileOwner::PlayerA) {
        for (const auto& monster : monsters_) hitIfNear(monster);
        for (const auto& card : opponentCardSystem_.cards()) hitIfNear(card);
    } else if (projectile.owner() == ProjectileOwner::PlayerB) {
        for (const auto& monster : monsters_) hitIfNear(monster);
        for (const auto& card : cardSystem_.cards()) hitIfNear(card);
    } else {
        for (const auto& card : cardSystem_.cards()) hitIfNear(card);
        for (const auto& card : opponentCardSystem_.cards()) hitIfNear(card);
    }
}

std::shared_ptr<Entity> BattleManager::findMonsterTarget(const std::shared_ptr<Monster>& monster) const {
    if (!monster || monster->attack() <= 0) return nullptr;

    std::shared_ptr<Entity> best;
    int bestDistance = 1000000;

    auto consider = [&](const std::shared_ptr<Card>& card) {
        if (!card || card->isDead()) return;
        const int distance = monster->position().manhattanDistanceTo(card->position());
        if (distance > monster->attackRange()) return;
        if (!best ||
            distance < bestDistance ||
            (distance == bestDistance && card->hp() < best->hp()) ||
            (distance == bestDistance && card->hp() == best->hp() && card->id() < best->id())) {
            best = card;
            bestDistance = distance;
        }
    };

    for (const auto& card : cardSystem_.cards()) consider(card);
    if (isPvp_) {
        for (const auto& card : opponentCardSystem_.cards()) consider(card);
    }
    return best;
}

void BattleManager::updateMonsterAttacks(double deltaSeconds) {
    constexpr double MonsterAttackIntervalSeconds = 1.4;

    for (const auto& monster : monsters_) {
        if (!monster || monster->isDead() || monster->escaped() || monster->attack() <= 0) continue;

        double& cooldown = monsterCooldowns_[monster->id()];
        cooldown += deltaSeconds;
        if (cooldown < MonsterAttackIntervalSeconds) continue;

        auto target = findMonsterTarget(monster);
        if (!target) continue;

        projectiles_.emplace_back(monster->id(),
                                  target,
                                  monster->position(),
                                  monster->attack(),
                                  ProjectileKind::Monster,
                                  ProjectileOwner::Monster,
                                  7.5,
                                  0);
        cooldown = 0.0;
    }
}

void BattleManager::removeResolvedMonsters() {
    // 使用 erase-remove 一次性移除所有死亡或逃逸怪物。
    monsters_.erase(std::remove_if(monsters_.begin(), monsters_.end(),
        [this](const std::shared_ptr<Monster>& monster) {
            if (!monster) return true;
            if (monster->isDead()) {
                // 死亡怪物发放资源奖励。
                monster->onDeath(resources_);
                ++defeatedMonsters_;
                monsterCooldowns_.erase(monster->id());
                return true;
            }
            if (monster->escaped()) {
                ++escapedMonsters_;
                // 逃逸怪物伤害基地。
                const MapGrid* grid = map_.gridAt(monster->position());
                if (grid && grid->terrainType() == TerrainType::CoreB) {
                    qDebug() << "[BattleManager] monster" << monster->id()
                             << "escaped to CoreB, damage" << monster->coreDamage();
                    opponentResources_.damageBase(monster->coreDamage());
                } else {
                    qDebug() << "[BattleManager] monster" << monster->id()
                             << "escaped to CoreA, damage" << monster->coreDamage();
                    resources_.damageBase(monster->coreDamage());
                }
                monsterCooldowns_.erase(monster->id());
                return true;
            }
            return false;
        }),
        monsters_.end());
}

void BattleManager::releaseDueSpawns() {
    auto it = pendingSpawns_.begin();
    while (it != pendingSpawns_.end()) {
        if (it->spawnTimeSeconds > waveElapsedSeconds_) {
            ++it;
            continue;
        }
        if (it->monster) {
            monsters_.push_back(it->monster);
        }
        it = pendingSpawns_.erase(it);
    }
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
