#include "core/map/Map.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace game::core {

Map::Map(int rows, int cols)
    : rows_(rows), cols_(cols), grids_(static_cast<std::size_t>(rows * cols)) {}

bool Map::inBounds(MapPosition position) const {
    return position.row >= 0 && position.row < rows_ &&
           position.col >= 0 && position.col < cols_;
}

void Map::resize(int rows, int cols, TerrainType terrain, int height) {
    rows_ = std::max(1, rows);
    cols_ = std::max(1, cols);
    grids_.assign(static_cast<std::size_t>(rows_ * cols_), MapGrid(terrain, height));
}

MapGrid* Map::gridAt(MapPosition position) {
    if (!inBounds(position)) return nullptr;
    return &grids_[static_cast<std::size_t>(position.row * cols_ + position.col)];
}

const MapGrid* Map::gridAt(MapPosition position) const {
    if (!inBounds(position)) return nullptr;
    return &grids_[static_cast<std::size_t>(position.row * cols_ + position.col)];
}

bool Map::setGrid(MapPosition position, TerrainType terrain, int height) {
    // 所有写操作都先通过 gridAt 做边界检查。
    MapGrid* grid = gridAt(position);
    if (!grid) return false;
    grid->setTerrainType(terrain);
    grid->setHeight(height);
    return true;
}

bool Map::setOccupied(MapPosition position, bool occupied, int occupantId) {
    MapGrid* grid = gridAt(position);
    if (!grid) return false;
    grid->setOccupied(occupied, occupantId);
    return true;
}

bool Map::clearOccupant(MapPosition position) {
    MapGrid* grid = gridAt(position);
    if (!grid) return false;
    grid->clearOccupant();
    return true;
}

bool Map::canDeployAt(MapPosition position) const {
    const MapGrid* grid = gridAt(position);
    return grid && grid->isDeployable();
}

bool Map::canWalkAt(MapPosition position) const {
    const MapGrid* grid = gridAt(position);
    return grid && grid->isWalkable();
}

std::vector<MapPosition> Map::neighbors4(MapPosition position) const {
    // 怪物寻路只走上下左右四方向，符合规则网格塔防的预期。
    const MapPosition candidates[] = {
        {position.row - 1, position.col},
        {position.row + 1, position.col},
        {position.row, position.col - 1},
        {position.row, position.col + 1}
    };

    std::vector<MapPosition> result;
    result.reserve(4);
    for (MapPosition next : candidates) {
        if (canWalkAt(next)) result.push_back(next);
    }
    return result;
}

std::vector<MapPosition> Map::deployableCells() const {
    std::vector<MapPosition> result;
    for (int row = 0; row < rows_; ++row) {
        for (int col = 0; col < cols_; ++col) {
            MapPosition p(row, col);
            if (canDeployAt(p)) result.push_back(p);
        }
    }
    return result;
}

bool Map::loadFromCsv(const std::string& path) {
    std::ifstream input(path);
    if (!input) return false;

    std::string line;
    while (std::getline(input, line)) {
        // 简单 CSV 解析：row,col,terrain,height。
        // 空行和无法识别的行会被跳过，保证配置文件局部错误不崩溃。
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string rowText, colText, terrainText, heightText;
        if (!std::getline(ss, rowText, ',')) continue;
        if (!std::getline(ss, colText, ',')) continue;
        if (!std::getline(ss, terrainText, ',')) continue;
        if (!std::getline(ss, heightText, ',')) heightText = "0";

        auto terrain = parseTerrain(terrainText);
        if (!terrain) continue;
        setGrid({std::stoi(rowText), std::stoi(colText)}, *terrain, std::stoi(heightText));
    }
    return true;
}

void Map::reset(TerrainType terrain, int height) {
    // 重建每个 MapGrid，同时清除占用状态。
    for (MapGrid& grid : grids_) {
        grid = MapGrid(terrain, height);
    }
}

std::optional<TerrainType> Map::parseTerrain(const std::string& value) {
    // 移除空白并统一大写，兼容 HIGH_GROUND/high_ground 等写法。
    std::string text;
    text.reserve(value.size());
    for (char ch : value) {
        if (!std::isspace(static_cast<unsigned char>(ch))) {
            text.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
        }
    }

    if (text == "PATH") return TerrainType::Path;
    if (text == "HIGH_GROUND" || text == "HIGHGROUND") return TerrainType::HighGround;
    if (text == "FLAT_LAND" || text == "FLATLAND") return TerrainType::FlatLand;
    if (text == "NO_DEPLOY" || text == "NODEPLOY") return TerrainType::NoDeploy;
    return std::nullopt;
}

} // namespace game::core
