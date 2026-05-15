#ifndef GAMEPROJECT_CORE_MAP_MAPGRID_H
#define GAMEPROJECT_CORE_MAP_MAPGRID_H

#include "core/base/CoreTypes.h"

namespace game::core {

// 单个地图格。
//
// 地图格保存地形、高度和占用状态。它不直接持有 Entity 指针，
// 而是保存 occupantId，这样 snapshot、网络同步和对象生命周期更清晰。
class MapGrid {
public:
    MapGrid(TerrainType terrain = TerrainType::FlatLand, int height = 0);

    TerrainType terrainType() const { return terrainType_; }
    TerrainType getType() const { return terrainType_; }
    int height() const { return height_; }
    int getHeight() const { return height_; }
    bool isOccupied() const { return occupied_; }
    bool getIsOccupied() const { return occupied_; }
    int occupantId() const { return occupantId_; }

    void setTerrainType(TerrainType type) { terrainType_ = type; }
    void setHeight(int height) { height_ = height; }

    // 设置占用状态。occupied=false 时 occupantId 会自动重置为 -1。
    void setOccupied(bool occupied, int occupantId = -1);
    void clearOccupant();

    // 怪物寻路可经过 Path/FlatLand/HighGround，不可经过 NoDeploy。
    bool isWalkable() const;

    // 卡牌只能部署在未占用的 FlatLand 或 HighGround 上。
    bool isDeployable() const;

private:
    // 地形类型。
    TerrainType terrainType_;
    // 当前是否有卡牌占用。
    bool occupied_;
    // 高度值。0 表示地面，1 表示高台。
    int height_;
    // 占用者 id；没有占用者时为 -1。
    int occupantId_;
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_MAP_MAPGRID_H
