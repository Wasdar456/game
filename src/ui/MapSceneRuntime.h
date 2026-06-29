#ifndef GAMEPROJECT_UI_MAPSCENERUNTIME_H
#define GAMEPROJECT_UI_MAPSCENERUNTIME_H

#include "core/map/Map.h"
#include "core/map/MapConfigLoader.h"
#include "ui/PvpMapLayout.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRectF>
#include <QString>

#include <set>
#include <string>
#include <vector>

namespace game::ui {

struct ResolvedMapScene {
    QString mapId;
    QString displayName;
    QString imageResourcePath;
    bool isPvp = false;
    bool usingFallback = false;
    game::core::LoadedMapConfig config;
};

inline QString locateProjectFile(const QString& relativePath)
{
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
        QFileInfo info(candidate);
        if (info.exists() && info.isFile()) return info.absoluteFilePath();
    }
    return {};
}

inline QString resolveMapImageResourcePath(const std::string& imageName)
{
    if (imageName.empty()) return {};
    const QString filename = QString::fromStdString(imageName);
    const QStringList candidates = {
        QString(":/images/artwork/%1").arg(filename),
        QString(":/images/maps/%1").arg(filename)
    };
    for (const QString& candidate : candidates) {
        if (QFileInfo::exists(candidate)) return candidate;
    }
    return {};
}

inline game::core::TerrainType terrainFromMapTileType(const std::string& type)
{
    if (type == "PATH_A" || type == "PATH_B" || type == "PATH_SHARED") {
        return game::core::TerrainType::Path;
    }
    if (type == "SPAWN_A" || type == "SPAWN_B") return game::core::TerrainType::SpawnPoint;
    if (type == "CORE_A") return game::core::TerrainType::CoreA;
    if (type == "CORE_B") return game::core::TerrainType::CoreB;
    if (type == "DEPLOY_A" || type == "DEPLOY_B" || type == "DEPLOY_NEUTRAL") {
        return game::core::TerrainType::FlatLand;
    }
    if (type == "HIGH_GROUND") return game::core::TerrainType::HighGround;
    return game::core::TerrainType::NoDeploy;
}

inline int terrainHeightFromMapTileType(const std::string& type)
{
    return type == "HIGH_GROUND" ? 1 : 0;
}

inline void appendUnique(std::vector<game::core::MapPosition>& target,
                         const game::core::MapPosition& position)
{
    if (std::find(target.begin(), target.end(), position) == target.end()) {
        target.push_back(position);
    }
}

inline void harvestTileSemantics(game::core::LoadedMapConfig& config)
{
    for (const auto& tile : config.tiles) {
        const game::core::MapPosition pos(tile.row, tile.col);
        if (tile.type == "DEPLOY_A") {
            appendUnique(config.deployA, pos);
        } else if (tile.type == "DEPLOY_B") {
            appendUnique(config.deployB, pos);
        } else if (tile.type == "DEPLOY_NEUTRAL") {
            appendUnique(config.deployNeutral, pos);
        } else if (tile.type == "VISION_BLOCK") {
            appendUnique(config.visionBlock, pos);
        } else if (tile.type == "RESOURCE") {
            appendUnique(config.resource, pos);
        }
    }
}

inline void applyLoadedMapToCoreMap(game::core::Map& map, const game::core::LoadedMapConfig& config)
{
    map.resize(config.rows, config.cols, game::core::TerrainType::NoDeploy, 0);
    for (const auto& tile : config.tiles) {
        map.setGrid({tile.row, tile.col},
                    terrainFromMapTileType(tile.type),
                    terrainHeightFromMapTileType(tile.type));
    }

    auto ensureRouteTiles = [&](const std::vector<std::vector<game::core::MapPosition>>& routes) {
        for (const auto& route : routes) {
            for (const auto& pos : route) {
                const auto* grid = map.gridAt(pos);
                if (grid && grid->terrainType() == game::core::TerrainType::NoDeploy) {
                    map.setGrid(pos, game::core::TerrainType::Path, 0);
                }
            }
        }
    };

    ensureRouteTiles(config.routesA);
    ensureRouteTiles(config.routesB);
}

inline QString fallbackMapDisplayName(const QString& mapId, bool isPvp)
{
    if (mapId == "lab_map_02" || mapId == "pvp_sunny_beach") return "Sunny Beach";
    if (mapId == "lab_map_01" || mapId == "pvp_office_panic") return "Office Panic";
    if (isPvp) return "PVP Trial";
    return "Jungle Ruins";
}

inline game::core::LoadedMapConfig fallbackPvpConfig(const QString& mapId)
{
    const auto layout = makePvpMapLayout(mapId.toStdString());
    game::core::LoadedMapConfig config;
    config.name = layout.id;
    config.displayName = fallbackMapDisplayName(mapId, true).toStdString();
    config.mode = "PVP";
    config.image = layout.image;
    config.rows = layout.rows;
    config.cols = layout.cols;
    config.cellSize = 48;
    config.cellSizeX = 48;
    config.cellSizeY = 48;
    config.unitVisualScale = layout.unitVisualScale;
    config.imageCrop.x = qRound(layout.backgroundSourceRect.x());
    config.imageCrop.y = qRound(layout.backgroundSourceRect.y());
    config.imageCrop.width = qRound(layout.backgroundSourceRect.width());
    config.imageCrop.height = qRound(layout.backgroundSourceRect.height());
    config.battleViewRect.x = qRound(layout.battleViewRect.x());
    config.battleViewRect.y = qRound(layout.battleViewRect.y());
    config.battleViewRect.width = qRound(layout.battleViewRect.width());
    config.battleViewRect.height = qRound(layout.battleViewRect.height());
    config.deployViewRect.x = qRound(layout.deployViewRect.x());
    config.deployViewRect.y = qRound(layout.deployViewRect.y());
    config.deployViewRect.width = qRound(layout.deployViewRect.width());
    config.deployViewRect.height = qRound(layout.deployViewRect.height());
    config.spawnA.push_back(layout.spawnA);
    config.spawnB.push_back(layout.spawnB);
    config.coreA.push_back(layout.coreA);
    config.coreB.push_back(layout.coreB);
    config.routesA.push_back(layout.pathToA);
    config.routesB.push_back(layout.pathToB);
    for (const auto& pos : layout.highGround) {
        config.tiles.push_back({pos.row, pos.col, "HIGH_GROUND"});
    }
    for (const auto& pos : layout.blocked) {
        config.tiles.push_back({pos.row, pos.col, "BLOCKED"});
    }
    for (const auto& pos : layout.pathToA) {
        config.tiles.push_back({pos.row, pos.col, "PATH_A"});
    }
    for (const auto& pos : layout.pathToB) {
        config.tiles.push_back({pos.row, pos.col, "PATH_B"});
    }
    config.tiles.push_back({layout.spawnA.row, layout.spawnA.col, "SPAWN_A"});
    config.tiles.push_back({layout.spawnB.row, layout.spawnB.col, "SPAWN_B"});
    config.tiles.push_back({layout.coreA.row, layout.coreA.col, "CORE_A"});
    config.tiles.push_back({layout.coreB.row, layout.coreB.col, "CORE_B"});
    harvestTileSemantics(config);
    return config;
}

inline ResolvedMapScene resolveMapScene(const QString& mapId, bool isPvp, QString* warning = nullptr)
{
    ResolvedMapScene scene;
    scene.mapId = mapId;
    scene.isPvp = isPvp;

    if (!mapId.isEmpty()) {
        const QString configPath = locateProjectFile(QString("assets/maps/%1.json").arg(mapId));
        if (!configPath.isEmpty()) {
            std::string error;
            if (game::core::MapConfigLoader::loadFromJson(configPath.toStdString(), scene.config, &error)) {
                harvestTileSemantics(scene.config);
                scene.displayName = !scene.config.displayName.empty()
                                        ? QString::fromStdString(scene.config.displayName)
                                        : fallbackMapDisplayName(mapId, isPvp);
                scene.imageResourcePath = resolveMapImageResourcePath(scene.config.image);
                if (isPvp) {
                    const auto fallback = makePvpMapLayout(mapId.toStdString());
                    if (!scene.config.battleViewRect.isValid()) {
                        scene.config.battleViewRect.x = qRound(fallback.battleViewRect.x());
                        scene.config.battleViewRect.y = qRound(fallback.battleViewRect.y());
                        scene.config.battleViewRect.width = qRound(fallback.battleViewRect.width());
                        scene.config.battleViewRect.height = qRound(fallback.battleViewRect.height());
                    }
                    if (!scene.config.deployViewRect.isValid()) {
                        scene.config.deployViewRect.x = qRound(fallback.deployViewRect.x());
                        scene.config.deployViewRect.y = qRound(fallback.deployViewRect.y());
                        scene.config.deployViewRect.width = qRound(fallback.deployViewRect.width());
                        scene.config.deployViewRect.height = qRound(fallback.deployViewRect.height());
                    }
                    if (scene.config.unitVisualScale <= 0.0) {
                        scene.config.unitVisualScale = fallback.unitVisualScale;
                    }
                    if (scene.config.imageCrop.width <= 0 || scene.config.imageCrop.height <= 0) {
                        scene.config.imageCrop.x = qRound(fallback.backgroundSourceRect.x());
                        scene.config.imageCrop.y = qRound(fallback.backgroundSourceRect.y());
                        scene.config.imageCrop.width = qRound(fallback.backgroundSourceRect.width());
                        scene.config.imageCrop.height = qRound(fallback.backgroundSourceRect.height());
                    }
                }
                return scene;
            }
            if (warning) {
                *warning = QString("failed to load %1: %2").arg(configPath, QString::fromStdString(error));
            }
        }
    }

    if (isPvp) {
        scene.usingFallback = true;
        scene.config = fallbackPvpConfig(mapId.isEmpty() ? QString("pvp_sunny_beach") : mapId);
        scene.displayName = fallbackMapDisplayName(mapId, true);
        scene.imageResourcePath = resolveMapImageResourcePath(scene.config.image);
        if (warning && warning->isEmpty()) {
            *warning = QString("using fallback PVP layout for %1").arg(mapId.isEmpty() ? QString("pvp_sunny_beach")
                                                                                       : mapId);
        }
    } else {
        scene.displayName = fallbackMapDisplayName(mapId, false);
    }
    return scene;
}

inline QRectF mapViewRectOrDefault(const game::core::ViewRect& rect,
                                   const QRectF& fallback)
{
    return rect.isValid()
               ? QRectF(rect.x, rect.y, rect.width, rect.height)
               : fallback;
}

inline game::core::MapPosition localSpawnForScene(const ResolvedMapScene& scene, bool isHost)
{
    const auto& spawns = isHost ? scene.config.spawnA : scene.config.spawnB;
    const auto& fallbackRoutes = isHost ? scene.config.routesA : scene.config.routesB;
    if (!spawns.empty()) return spawns.front();
    if (!fallbackRoutes.empty() && !fallbackRoutes.front().empty()) {
        return fallbackRoutes.front().front();
    }
    return {};
}

inline game::core::MapPosition localCoreForScene(const ResolvedMapScene& scene, bool isHost)
{
    const auto& cores = isHost ? scene.config.coreA : scene.config.coreB;
    const auto& fallbackRoutes = isHost ? scene.config.routesA : scene.config.routesB;
    if (!cores.empty()) return cores.front();
    if (!fallbackRoutes.empty() && !fallbackRoutes.front().empty()) {
        return fallbackRoutes.front().back();
    }
    return {};
}

inline std::vector<std::vector<game::core::MapPosition>> combinedRoutesForScene(
    const ResolvedMapScene& scene)
{
    std::vector<std::vector<game::core::MapPosition>> result = scene.config.routesA;
    result.insert(result.end(), scene.config.routesB.begin(), scene.config.routesB.end());
    return result;
}

inline std::vector<game::core::MapPosition> deploymentZoneForSide(const ResolvedMapScene& scene,
                                                                  bool isHost)
{
    std::set<game::core::MapPosition> unique;
    for (const auto& pos : scene.config.deployNeutral) {
        unique.insert(pos);
    }
    const auto& sideDeploy = isHost ? scene.config.deployA : scene.config.deployB;
    for (const auto& pos : sideDeploy) {
        unique.insert(pos);
    }
    return {unique.begin(), unique.end()};
}

inline bool isSceneDeploymentCellForSide(const ResolvedMapScene& scene,
                                         bool isHost,
                                         game::core::MapPosition position)
{
    if (scene.config.deployA.empty()
        && scene.config.deployB.empty()
        && scene.config.deployNeutral.empty()) {
        return isPvpDeploymentCellForHost(isHost, position);
    }

    const auto matches = [&](const std::vector<game::core::MapPosition>& positions) {
        return std::find(positions.begin(), positions.end(), position) != positions.end();
    };
    return matches(scene.config.deployNeutral)
           || matches(isHost ? scene.config.deployA : scene.config.deployB);
}

inline std::vector<game::core::MapPosition> coreVisionBlockersForScene(const ResolvedMapScene& scene)
{
    return scene.config.visionBlock;
}

} // namespace game::ui

#endif // GAMEPROJECT_UI_MAPSCENERUNTIME_H
