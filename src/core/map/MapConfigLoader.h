#ifndef GAMEPROJECT_CORE_MAP_MAPCONFIGLOADER_H
#define GAMEPROJECT_CORE_MAP_MAPCONFIGLOADER_H

#include "core/map/MapPosition.h"
#include <string>
#include <vector>

namespace game::core {

struct LoadedMapTile {
    int row = 0;
    int col = 0;
    std::string type;
};

struct ImageCrop {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct ImageOffset {
    int x = 0;
    int y = 0;
};

struct ViewRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool isValid() const { return width > 0 && height > 0; }
};

struct LoadedMapConfig {
    std::string name;
    std::string displayName;
    std::string mode = "PVE";
    std::string image;
    int rows = 0;
    int cols = 0;
    int cellSize = 0;
    int cellSizeX = 0;
    int cellSizeY = 0;
    double unitVisualScale = 0.0;
    ImageCrop imageCrop;
    ImageOffset imageOffset;
    ViewRect battleViewRect;
    ViewRect deployViewRect;
    std::vector<LoadedMapTile> tiles;
    std::vector<std::vector<MapPosition>> routesA;
    std::vector<std::vector<MapPosition>> routesB;
    std::vector<MapPosition> spawnA;
    std::vector<MapPosition> spawnB;
    std::vector<MapPosition> coreA;
    std::vector<MapPosition> coreB;
    std::vector<MapPosition> deployA;
    std::vector<MapPosition> deployB;
    std::vector<MapPosition> deployNeutral;
    std::vector<MapPosition> visionBlock;
    std::vector<MapPosition> resource;
};

class MapConfigLoader {
public:
    static bool loadFromJson(const std::string& path, LoadedMapConfig& config, std::string* error = nullptr);
};

} // namespace game::core

#endif // GAMEPROJECT_CORE_MAP_MAPCONFIGLOADER_H
