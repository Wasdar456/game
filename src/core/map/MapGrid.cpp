#include "core/map/MapGrid.h"

namespace game::core {

MapGrid::MapGrid(TerrainType terrain, int height)
    : terrainType_(terrain), occupied_(false), height_(height), occupantId_(-1) {}

void MapGrid::setOccupied(bool occupied, int occupantId) {
    occupied_ = occupied;
    occupantId_ = occupied ? occupantId : -1;
}

void MapGrid::clearOccupant() {
    setOccupied(false, -1);
}

bool MapGrid::isWalkable() const {
    return terrainType_ == TerrainType::Path ||
           terrainType_ == TerrainType::FlatLand ||
           terrainType_ == TerrainType::HighGround ||
           terrainType_ == TerrainType::SpawnPoint ||
           terrainType_ == TerrainType::CoreA ||
           terrainType_ == TerrainType::CoreB;
}

bool MapGrid::isDeployable() const {
    return !occupied_ &&
           (terrainType_ == TerrainType::FlatLand ||
            terrainType_ == TerrainType::HighGround);
}

} // namespace game::core
