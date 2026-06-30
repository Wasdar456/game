#ifndef GAMEPROJECT_CORE_MAP_MAP_H
#define GAMEPROJECT_CORE_MAP_MAP_H

#include "core/base/Constants.h"
#include "core/map/MapGrid.h"
#include "core/map/MapPosition.h"
#include <optional>
#include <string>
#include <vector>

namespace game::core {

// 地图容器。
//
// Map 负责管理二维 MapGrid 数组，提供边界检查、部署检查、
// 行走检查、四方向邻居查询和 CSV 加载。战斗系统只通过这些接口
// 操作地图，避免各模块直接处理数组下标。
class Map {
public:
    Map(int rows = constants::DefaultMapRows, int cols = constants::DefaultMapCols);

    int rows() const { return rows_; }
    int cols() const { return cols_; }
    bool inBounds(MapPosition position) const;

    // 重建地图尺寸，并将所有格子初始化为指定地形。
    void resize(int rows, int cols, TerrainType terrain = TerrainType::FlatLand, int height = 0);

    MapGrid* gridAt(MapPosition position);
    const MapGrid* gridAt(MapPosition position) const;

    // 设置指定格子的地形和高度。越界时返回 false。
    bool setGrid(MapPosition position, TerrainType terrain, int height = 0);

    // 设置/清除占用信息，用于部署、移动和撤回。
    bool setOccupied(MapPosition position, bool occupied, int occupantId = -1);
    bool clearOccupant(MapPosition position);

    // 部署合法性和行走合法性的统一入口。
    bool canDeployAt(MapPosition position) const;
    bool canWalkAt(MapPosition position) const;

    // A* 寻路使用的四方向邻居。
    std::vector<MapPosition> neighbors4(MapPosition position) const;

    // UI 高亮可部署格时可直接使用。
    std::vector<MapPosition> deployableCells() const;

    // 从 CSV 加载地图。格式：row,col,terrain,height。
    bool loadFromCsv(const std::string& path);

    // 重置整张地图为同一种地形，常用于测试或切换关卡。
    void reset(TerrainType terrain = TerrainType::FlatLand, int height = 0);

private:
    // 将 CSV 文本转换为 TerrainType，支持常见大小写和下划线形式。
    static std::optional<TerrainType> parseTerrain(const std::string& value);

    int rows_;
    int cols_;
    std::vector<MapGrid> grids_;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_MAP_MAP_H
