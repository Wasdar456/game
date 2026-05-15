#ifndef GAMEPROJECT_CORE_SNAPSHOT_MAPSNAPSHOT_H
#define GAMEPROJECT_CORE_SNAPSHOT_MAPSNAPSHOT_H

#include "core/base/CoreTypes.h"
#include <vector>

namespace game::core {

// 单个地图格的只读展示数据。
struct GridSnapshot {
    // 网格坐标。
    int row = 0;
    int col = 0;
    // 地形、高度和占用信息。
    TerrainType terrain = TerrainType::FlatLand;
    int height = 0;
    bool occupied = false;
    int occupantId = -1;
};

// 整张地图的只读展示数据。
struct MapSnapshot {
    int rows = 0;
    int cols = 0;
    std::vector<GridSnapshot> grids;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_SNAPSHOT_MAPSNAPSHOT_H
