#ifndef GAMEPROJECT_CORE_MAP_MAPPOSITION_H
#define GAMEPROJECT_CORE_MAP_MAPPOSITION_H

#include <cstdlib>
#include <functional>

namespace game::core {

// 网格坐标。全项目统一使用 row/col，避免 X/Y 在 UI 和逻辑层中混淆。
struct MapPosition {
    int row = 0;
    int col = 0;

    MapPosition() = default;
    MapPosition(int r, int c) : row(r), col(c) {}

    bool operator==(const MapPosition& other) const {
        return row == other.row && col == other.col;
    }

    bool operator!=(const MapPosition& other) const {
        return !(*this == other);
    }

    bool operator<(const MapPosition& other) const {
        return row < other.row || (row == other.row && col < other.col);
    }

    // 塔防网格上的移动、射程、瞬移都使用曼哈顿距离。
    int manhattanDistanceTo(const MapPosition& other) const {
        return std::abs(row - other.row) + std::abs(col - other.col);
    }
};

} // namespace game::core

namespace std {
// 让 MapPosition 可以作为 unordered_map 的 key，A* 寻路会用到。
template <>
struct hash<game::core::MapPosition> {
    std::size_t operator()(const game::core::MapPosition& p) const noexcept {
        return (static_cast<std::size_t>(p.row) << 32) ^
               static_cast<std::size_t>(p.col);
    }
};
} // namespace std

#endif // GAMEPROJECT_CORE_MAP_MAPPOSITION_H
