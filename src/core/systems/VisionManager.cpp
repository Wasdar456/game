#include "core/systems/VisionManager.h"
#include <cmath>

namespace game::core {

VisionManager::VisionManager() {}

void VisionManager::initDefaultVision(MapPosition coreAPos, MapPosition coreBPos) {
    // A方核心周围 3x3 区域
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            visionA_.insert(MapPosition(coreAPos.row + dr, coreAPos.col + dc));
        }
    }

    // B方核心周围 3x3 区域
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            visionB_.insert(MapPosition(coreBPos.row + dr, coreBPos.col + dc));
        }
    }
}

void VisionManager::updateVision(const Map& map,
                                  const std::vector<std::shared_ptr<Card>>& cardsA,
                                  const std::vector<std::shared_ptr<Card>>& cardsB) {
    // 保留核心视野
    std::set<MapPosition> coreVisionA;
    std::set<MapPosition> coreVisionB;

    // 从现有视野中提取核心视野（核心周围3x3）
    for (const auto& pos : visionA_) {
        // 检查是否在核心周围
        if (pos.row >= 9 && pos.row <= 11 && pos.col >= 0 && pos.col <= 2) {
            coreVisionA.insert(pos);
        }
    }
    for (const auto& pos : visionB_) {
        if (pos.row >= 9 && pos.row <= 11 && pos.col >= 15 && pos.col <= 17) {
            coreVisionB.insert(pos);
        }
    }

    // 重置视野
    visionA_ = coreVisionA;
    visionB_ = coreVisionB;

    // 添加A方单位的视野
    for (const auto& card : cardsA) {
        if (card && !card->isDead()) {
            int range = card->attackRange() + 1;  // 视野 = 攻击范围 + 1
            // 高台单位视野 + 1
            const MapGrid* grid = map.gridAt(card->position());
            if (grid && grid->height() > 0) {
                range += 1;
            }
            addVisionAround(visionA_, card->position(), range, map);
        }
    }

    // 添加B方单位的视野
    for (const auto& card : cardsB) {
        if (card && !card->isDead()) {
            int range = card->attackRange() + 1;
            const MapGrid* grid = map.gridAt(card->position());
            if (grid && grid->height() > 0) {
                range += 1;
            }
            addVisionAround(visionB_, card->position(), range, map);
        }
    }
}

bool VisionManager::isInVisionA(MapPosition pos) const {
    return visionA_.find(pos) != visionA_.end();
}

bool VisionManager::isInVisionB(MapPosition pos) const {
    return visionB_.find(pos) != visionB_.end();
}

std::vector<MapPosition> VisionManager::getDeployableCellsA(const Map& map) const {
    std::vector<MapPosition> result;
    for (const auto& pos : visionA_) {
        if (map.canDeployAt(pos)) {
            result.push_back(pos);
        }
    }
    return result;
}

std::vector<MapPosition> VisionManager::getDeployableCellsB(const Map& map) const {
    std::vector<MapPosition> result;
    for (const auto& pos : visionB_) {
        if (map.canDeployAt(pos)) {
            result.push_back(pos);
        }
    }
    return result;
}

void VisionManager::addVisionAround(std::set<MapPosition>& vision,
                                     MapPosition center,
                                     int range,
                                     const Map& map) {
    // 添加曼哈顿距离内的所有格子
    for (int dr = -range; dr <= range; ++dr) {
        for (int dc = -range; dc <= range; ++dc) {
            if (std::abs(dr) + std::abs(dc) <= range) {
                MapPosition pos(center.row + dr, center.col + dc);
                if (map.inBounds(pos)) {
                    vision.insert(pos);
                }
            }
        }
    }
}

} // namespace game::core
