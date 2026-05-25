#ifndef GAMEPROJECT_CORE_MAP_ASTAR_H
#define GAMEPROJECT_CORE_MAP_ASTAR_H

#include "core/map/Map.h"
#include <vector>

namespace game::core {

// A* 网格寻路。
//
// 当前实现只使用曼哈顿启发式和四方向移动，适合规则塔防地图。
// 如果以后需要斜向移动或地形权重，可以在 Map::neighbors4 或这里扩展。
class AStar {
public:
    // 返回包含 start 和 goal 的完整路径；找不到路径时返回空数组。
    static std::vector<MapPosition> findPath(const Map& map,
                                             MapPosition start,
                                             MapPosition goal);
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_MAP_ASTAR_H
