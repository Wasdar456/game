#include "core/systems/VisionManager.h"
#include <cmath>
#include <queue>

namespace game::core {

VisionManager::VisionManager() {}

void VisionManager::clear() {
    visionA_.clear();
    visionB_.clear();
    visionBlocks_.clear();
    coreAPos_ = {};
    coreBPos_ = {};
    hasCoreVision_ = false;
}

void VisionManager::initDefaultVision(MapPosition coreAPos, MapPosition coreBPos) {
    visionA_.clear();
    visionB_.clear();
    coreAPos_ = coreAPos;
    coreBPos_ = coreBPos;
    hasCoreVision_ = true;
    addCoreVision(visionA_, coreAPos_);
    addCoreVision(visionB_, coreBPos_);
}

void VisionManager::updateVision(const Map& map,
                                  const std::vector<std::shared_ptr<Card>>& cardsA,
                                  const std::vector<std::shared_ptr<Card>>& cardsB) {
    visionA_.clear();
    visionB_.clear();
    if (hasCoreVision_) {
        addCoreVision(visionA_, coreAPos_);
        addCoreVision(visionB_, coreBPos_);
    }

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

void VisionManager::setVisionBlocks(std::vector<MapPosition> blocks) {
    visionBlocks_ = {blocks.begin(), blocks.end()};
}

void VisionManager::addVisionAround(std::set<MapPosition>& vision,
                                     MapPosition center,
                                     int range,
                                     const Map& map) {
    struct Node {
        MapPosition pos;
        int distanceLeft = 0;
    };

    std::queue<Node> frontier;
    std::set<MapPosition> visited;
    frontier.push({center, range});
    visited.insert(center);

    while (!frontier.empty()) {
        const Node current = frontier.front();
        frontier.pop();
        if (!map.inBounds(current.pos)) continue;

        vision.insert(current.pos);
        const bool blocksFurther = current.pos != center
                                   && visionBlocks_.find(current.pos) != visionBlocks_.end();
        if (current.distanceLeft <= 0 || blocksFurther) {
            continue;
        }

        const MapPosition neighbors[] = {
            {current.pos.row - 1, current.pos.col},
            {current.pos.row + 1, current.pos.col},
            {current.pos.row, current.pos.col - 1},
            {current.pos.row, current.pos.col + 1}
        };
        for (const auto& next : neighbors) {
            if (!map.inBounds(next) || visited.find(next) != visited.end()) continue;
            visited.insert(next);
            frontier.push({next, current.distanceLeft - 1});
        }
    }
}

void VisionManager::addCoreVision(std::set<MapPosition>& vision, MapPosition center) {
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            vision.insert(MapPosition(center.row + dr, center.col + dc));
        }
    }
}

} // namespace game::core
