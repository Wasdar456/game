#include "core/map/MapConfigLoader.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <algorithm>

namespace game::core {

namespace {

MapPosition readPosition(const QJsonObject& object) {
    return {
        object.value("row").toInt(),
        object.value("col").toInt()
    };
}

std::vector<MapPosition> readPointArray(const QJsonArray& array) {
    std::vector<MapPosition> result;
    result.reserve(static_cast<std::size_t>(array.size()));
    for (const QJsonValue& value : array) {
        if (value.isObject()) {
            result.push_back(readPosition(value.toObject()));
        }
    }
    return result;
}

std::vector<std::vector<MapPosition>> readRoutes(const QJsonValue& value) {
    std::vector<std::vector<MapPosition>> routes;
    if (!value.isArray()) return routes;

    const QJsonArray array = value.toArray();
    for (const QJsonValue& routeValue : array) {
        if (routeValue.isObject()) {
            const QJsonObject routeObject = routeValue.toObject();
            routes.push_back(readPointArray(routeObject.value("path").toArray()));
        } else if (routeValue.isArray()) {
            routes.push_back(readPointArray(routeValue.toArray()));
        }
    }

    routes.erase(std::remove_if(routes.begin(), routes.end(),
                                [](const std::vector<MapPosition>& route) {
                                    return route.empty();
                                }),
                 routes.end());
    return routes;
}

} // namespace

bool MapConfigLoader::loadFromJson(const std::string& path, LoadedMapConfig& config, std::string* error) {
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString().toStdString();
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = parseError.errorString().toStdString();
        return false;
    }

    const QJsonObject root = document.object();
    LoadedMapConfig loaded;
    loaded.name = root.value("name").toString().toStdString();
    loaded.mode = root.value("mode").toString("PVE").toUpper().toStdString();
    loaded.image = root.value("image").toString().toStdString();

    const QJsonObject grid = root.value("grid").toObject();
    loaded.rows = grid.value("rows").toInt();
    loaded.cols = grid.value("cols").toInt();
    loaded.cellSize = grid.value("cellSize").toInt();
    loaded.cellSizeX = grid.value("cellSizeX").toInt(loaded.cellSize);
    loaded.cellSizeY = grid.value("cellSizeY").toInt(loaded.cellSize);
    if (loaded.rows <= 0 || loaded.cols <= 0) {
        if (error) *error = "grid.rows/grid.cols must be positive";
        return false;
    }

    const QJsonObject imageCropObj = root.value("imageCrop").toObject();
    if (!imageCropObj.isEmpty()) {
        loaded.imageCrop.x = imageCropObj.value("x").toInt();
        loaded.imageCrop.y = imageCropObj.value("y").toInt();
        loaded.imageCrop.width = imageCropObj.value("width").toInt();
        loaded.imageCrop.height = imageCropObj.value("height").toInt();
    }

    const QJsonObject imageOffsetObj = root.value("imageOffset").toObject();
    if (!imageOffsetObj.isEmpty()) {
        loaded.imageOffset.x = imageOffsetObj.value("x").toInt();
        loaded.imageOffset.y = imageOffsetObj.value("y").toInt();
    }

    const QJsonArray tiles = root.value("tiles").toArray();
    loaded.tiles.reserve(static_cast<std::size_t>(tiles.size()));
    for (const QJsonValue& value : tiles) {
        if (!value.isObject()) continue;
        const QJsonObject tile = value.toObject();
        LoadedMapTile loadedTile;
        loadedTile.row = tile.value("row").toInt(-1);
        loadedTile.col = tile.value("col").toInt(-1);
        loadedTile.type = tile.value("type").toString("EMPTY").toUpper().toStdString();
        if (loadedTile.row < 0 || loadedTile.row >= loaded.rows ||
            loadedTile.col < 0 || loadedTile.col >= loaded.cols) {
            continue;
        }
        loaded.tiles.push_back(std::move(loadedTile));
    }

    const QJsonObject points = root.value("points").toObject();
    loaded.spawnA = readPointArray(points.value("SPAWN_A").toArray());
    loaded.spawnB = readPointArray(points.value("SPAWN_B").toArray());
    loaded.coreA = readPointArray(points.value("CORE_A").toArray());
    loaded.coreB = readPointArray(points.value("CORE_B").toArray());

    const QJsonObject routes = root.value("routes").toObject();
    loaded.routesA = readRoutes(routes.value("A"));
    loaded.routesB = readRoutes(routes.value("B"));

    config = std::move(loaded);
    return true;
}

} // namespace game::core
