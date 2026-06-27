#ifndef GAMEPROJECT_UI_PVPMAPLAYOUT_H
#define GAMEPROJECT_UI_PVPMAPLAYOUT_H

#include "core/map/Map.h"
#include "core/map/MapPosition.h"

#include <vector>

namespace game::ui {

struct PvpMapLayout {
    int rows = 10;
    int cols = 28;
    core::MapPosition spawnA;
    core::MapPosition spawnB;
    core::MapPosition coreA;
    core::MapPosition coreB;
    std::vector<core::MapPosition> pathToA;
    std::vector<core::MapPosition> pathToB;
    std::vector<core::MapPosition> highGround;
    std::vector<core::MapPosition> blocked;
};

inline PvpMapLayout makePvpMapLayout()
{
    PvpMapLayout layout;
    layout.spawnA = {1, 13};
    layout.spawnB = {8, 14};
    layout.coreA = {6, 2};
    layout.coreB = {6, 25};

    layout.pathToA = {
        layout.spawnA,
        {2, 13}, {3, 13}, {4, 13}, {5, 13},
        {5, 12}, {5, 11}, {6, 11}, {6, 10}, {6, 9},
        {6, 8}, {6, 7}, {6, 6}, {6, 5}, {6, 4}, {6, 3},
        layout.coreA
    };
    layout.pathToB = {
        layout.spawnB,
        {7, 14}, {6, 14}, {5, 14},
        {5, 15}, {5, 16}, {6, 16}, {6, 17}, {6, 18},
        {6, 19}, {6, 20}, {6, 21}, {6, 22}, {6, 23}, {6, 24},
        layout.coreB
    };

    layout.highGround = {
        {2, 8}, {3, 8}, {4, 7}, {7, 7},
        {2, 19}, {3, 19}, {4, 20}, {7, 20}
    };
    layout.blocked = {
        {0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4},
        {0, 23}, {0, 24}, {0, 25}, {0, 26}, {0, 27},
        {1, 1}, {1, 2}, {1, 3}, {1, 24}, {1, 25}, {1, 26},
        {2, 1}, {2, 2}, {2, 3}, {2, 24}, {2, 25}, {2, 26},
        {3, 2}, {3, 25},
        {7, 3}, {7, 4}, {7, 23}, {7, 24},
        {8, 2}, {8, 3}, {8, 4}, {8, 23}, {8, 24}, {8, 25},
        {9, 0}, {9, 1}, {9, 2}, {9, 3}, {9, 4},
        {9, 23}, {9, 24}, {9, 25}, {9, 26}, {9, 27}
    };
    return layout;
}

inline bool isPvpDeploymentCellForHost(bool isHost, core::MapPosition position)
{
    constexpr int SharedLeftCol = 13;
    constexpr int SharedRightCol = 14;
    return isHost ? position.col <= SharedRightCol
                  : position.col >= SharedLeftCol;
}

inline void applyPvpMapLayout(core::Map& map, const PvpMapLayout& layout)
{
    map.resize(layout.rows, layout.cols, core::TerrainType::FlatLand, 0);

    for (const auto& pos : layout.blocked) {
        map.setGrid(pos, core::TerrainType::NoDeploy, 0);
    }
    for (const auto& pos : layout.highGround) {
        map.setGrid(pos, core::TerrainType::HighGround, 1);
    }
    for (const auto& pos : layout.pathToA) {
        map.setGrid(pos, core::TerrainType::Path, 0);
    }
    for (const auto& pos : layout.pathToB) {
        map.setGrid(pos, core::TerrainType::Path, 0);
    }

    map.setGrid(layout.spawnA, core::TerrainType::SpawnPoint, 0);
    map.setGrid(layout.spawnB, core::TerrainType::SpawnPoint, 0);
    map.setGrid(layout.coreA, core::TerrainType::CoreA, 0);
    map.setGrid(layout.coreB, core::TerrainType::CoreB, 0);
}

} // namespace game::ui

#endif // GAMEPROJECT_UI_PVPMAPLAYOUT_H
