#ifndef GAMEPROJECT_UI_PVPMAPLAYOUT_H
#define GAMEPROJECT_UI_PVPMAPLAYOUT_H

#include "core/map/MapConfigLoader.h"
#include "core/map/Map.h"
#include "core/map/MapPosition.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QRectF>

#include <string>
#include <vector>

namespace game::ui {

struct PvpMapLayout {
    int rows = 10;
    int cols = 28;
    std::string id = "pvp_sunny_beach";
    std::string image = "battle_pvp.png";
    QRectF backgroundSourceRect = QRectF(0, 96, 1672, 604);
    QRectF battleViewRect = QRectF(174, 126, 1324, 552);
    QRectF deployViewRect = QRectF(174, 126, 1324, 552);
    double unitVisualScale = 1.0;
    core::MapPosition spawnA;
    core::MapPosition spawnB;
    core::MapPosition coreA;
    core::MapPosition coreB;
    std::vector<core::MapPosition> pathToA;
    std::vector<core::MapPosition> pathToB;
    std::vector<core::MapPosition> highGround;
    std::vector<core::MapPosition> blocked;
};

inline PvpMapLayout makeSunnyBeachPvpMapLayout()
{
    PvpMapLayout layout;
    layout.id = "pvp_sunny_beach";
    layout.image = "battle_pvp.png";
    layout.backgroundSourceRect = QRectF(0, 96, 1672, 604);
    layout.battleViewRect = QRectF(174, 126, 1324, 552);
    layout.deployViewRect = QRectF(174, 126, 1324, 552);
    layout.unitVisualScale = 1.0;
    layout.spawnA = {0, 13};
    layout.spawnB = {9, 14};
    layout.coreA = {5, 3};
    layout.coreB = {5, 23};

    layout.pathToA = {
        layout.spawnA,
        {1, 13}, {2, 13}, {3, 13}, {4, 13}, {5, 13},
        {5, 12}, {5, 11}, {5, 10}, {5, 9}, {5, 8},
        {5, 7}, {5, 6}, {5, 5}, {5, 4},
        layout.coreA
    };
    layout.pathToB = {
        layout.spawnB,
        {8, 14}, {7, 14}, {6, 14}, {5, 14},
        {5, 15}, {5, 16}, {5, 17}, {5, 18}, {5, 19},
        {5, 20}, {5, 21}, {5, 22},
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

inline PvpMapLayout makeOfficePanicPvpMapLayout()
{
    PvpMapLayout layout;
    layout.id = "pvp_office_panic";
    layout.image = "battle_pvp_office_map.png";
    layout.backgroundSourceRect = QRectF(0, 120, 1672, 604);
    layout.battleViewRect = QRectF(174, 126, 1324, 552);
    layout.deployViewRect = QRectF(174, 126, 1324, 552);
    layout.unitVisualScale = 1.12;
    layout.spawnA = {1, 14};
    layout.spawnB = {8, 14};
    layout.coreA = {6, 3};
    layout.coreB = {6, 24};

    layout.pathToA = {
        layout.spawnA,
        {2, 14}, {3, 14}, {4, 14}, {5, 14},
        {5, 13}, {5, 12}, {5, 11}, {5, 10}, {5, 9}, {5, 8},
        {5, 7}, {5, 6}, {5, 5}, {6, 5}, {6, 4},
        layout.coreA
    };
    layout.pathToB = {
        layout.spawnB,
        {7, 14}, {6, 14}, {5, 14},
        {5, 15}, {5, 16}, {5, 17}, {5, 18}, {5, 19}, {5, 20},
        {5, 21}, {5, 22}, {6, 22}, {6, 23},
        layout.coreB
    };

    layout.highGround = {
        {2, 7}, {3, 7}, {6, 7}, {7, 7},
        {2, 20}, {3, 20}, {6, 20}, {7, 20}
    };
    layout.blocked = {
        {0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4},
        {0, 23}, {0, 24}, {0, 25}, {0, 26}, {0, 27},
        {1, 0}, {1, 1}, {1, 2}, {1, 3}, {1, 24}, {1, 25}, {1, 26}, {1, 27},
        {2, 1}, {2, 2}, {2, 3}, {2, 4}, {2, 10}, {2, 11}, {2, 16}, {2, 17},
        {2, 23}, {2, 24}, {2, 25}, {2, 26},
        {3, 2}, {3, 3}, {3, 24}, {3, 25},
        {6, 2}, {6, 3}, {6, 24}, {6, 25},
        {7, 1}, {7, 2}, {7, 3}, {7, 4}, {7, 10}, {7, 11}, {7, 16}, {7, 17},
        {7, 23}, {7, 24}, {7, 25}, {7, 26},
        {8, 0}, {8, 1}, {8, 2}, {8, 3}, {8, 24}, {8, 25}, {8, 26}, {8, 27},
        {9, 0}, {9, 1}, {9, 2}, {9, 3}, {9, 4},
        {9, 23}, {9, 24}, {9, 25}, {9, 26}, {9, 27}
    };
    return layout;
}

inline QString findPvpMapConfigFile(const std::string& mapId)
{
    const QString relativePath = QString("assets/maps/%1.json")
                                     .arg(QString::fromStdString(mapId));
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString cwd = QDir::currentPath();
    const QStringList candidates = {
        QDir(cwd).filePath(relativePath),
        QDir(appDir).filePath(relativePath),
        QDir(appDir).filePath("../" + relativePath),
        QDir(appDir).filePath("../../" + relativePath),
        QDir(appDir).filePath("../../../" + relativePath)
    };
    for (const QString& candidate : candidates) {
        const QFileInfo info(candidate);
        if (info.exists() && info.isFile()) {
            return info.absoluteFilePath();
        }
    }
    return {};
}

inline game::core::TerrainType pvpTerrainFromMapTile(const std::string& type)
{
    if (type == "PATH_A" || type == "PATH_B" || type == "PATH_SHARED") {
        return game::core::TerrainType::Path;
    }
    if (type == "SPAWN_A" || type == "SPAWN_B") return game::core::TerrainType::SpawnPoint;
    if (type == "CORE_A") return game::core::TerrainType::CoreA;
    if (type == "CORE_B") return game::core::TerrainType::CoreB;
    if (type == "HIGH_GROUND") return game::core::TerrainType::HighGround;
    if (type == "DEPLOY_A" || type == "DEPLOY_B" || type == "DEPLOY_NEUTRAL") {
        return game::core::TerrainType::FlatLand;
    }
    return game::core::TerrainType::NoDeploy;
}

inline int pvpTerrainHeightFromMapTile(const std::string& type)
{
    return type == "HIGH_GROUND" ? 1 : 0;
}

inline PvpMapLayout makePvpMapLayoutFromConfig(const game::core::LoadedMapConfig& config)
{
    PvpMapLayout layout;
    layout.id = config.name.empty() ? "pvp_sunny_beach" : config.name;
    layout.image = config.image.empty()
                       ? (layout.id == "pvp_office_panic"
                              ? "battle_pvp_office_map.png"
                              : "battle_pvp.png")
                       : config.image;
    layout.rows = config.rows;
    layout.cols = config.cols;
    if (config.imageCrop.width > 0 && config.imageCrop.height > 0) {
        layout.backgroundSourceRect = QRectF(config.imageCrop.x,
                                             config.imageCrop.y,
                                             config.imageCrop.width,
                                             config.imageCrop.height);
    } else {
        layout.backgroundSourceRect = layout.id == "pvp_office_panic"
                                          ? QRectF(0, 120, 1672, 604)
                                          : QRectF(0, 96, 1672, 604);
    }
    layout.battleViewRect = QRectF(174, 126, 1324, 552);
    layout.deployViewRect = layout.battleViewRect;
    layout.unitVisualScale = layout.id == "pvp_office_panic" ? 1.12 : 1.0;

    if (!config.spawnA.empty()) layout.spawnA = config.spawnA.front();
    if (!config.spawnB.empty()) layout.spawnB = config.spawnB.front();
    if (!config.coreA.empty()) layout.coreA = config.coreA.front();
    if (!config.coreB.empty()) layout.coreB = config.coreB.front();
    if (!config.routesA.empty()) layout.pathToA = config.routesA.front();
    if (!config.routesB.empty()) layout.pathToB = config.routesB.front();

    for (const auto& tile : config.tiles) {
        const game::core::MapPosition position(tile.row, tile.col);
        const auto terrain = pvpTerrainFromMapTile(tile.type);
        if (terrain == game::core::TerrainType::NoDeploy) {
            layout.blocked.push_back(position);
        } else if (terrain == game::core::TerrainType::HighGround) {
            layout.highGround.push_back(position);
        }
    }

    return layout;
}

inline PvpMapLayout makePvpMapLayout(const std::string& mapId = "pvp_sunny_beach")
{
    const QString configPath = findPvpMapConfigFile(mapId);
    game::core::LoadedMapConfig config;
    std::string error;
    if (!configPath.isEmpty()
        && game::core::MapConfigLoader::loadFromJson(configPath.toStdString(),
                                                     config,
                                                     &error)
        && config.mode == "PVP"
        && !config.routesA.empty()
        && !config.routesB.empty()
        && !config.spawnA.empty()
        && !config.spawnB.empty()
        && !config.coreA.empty()
        && !config.coreB.empty()) {
        return makePvpMapLayoutFromConfig(config);
    }

    return mapId == "pvp_office_panic"
               ? makeOfficePanicPvpMapLayout()
               : makeSunnyBeachPvpMapLayout();
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
