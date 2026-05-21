#include "core/map/AStar.h"
#include <algorithm>
#include <limits>
#include <queue>
#include <unordered_map>

namespace game::core {

namespace {
struct Node {
    MapPosition position;
    int fScore = 0;

    bool operator>(const Node& other) const {
        return fScore > other.fScore;
    }
};
} // namespace

std::vector<MapPosition> AStar::findPath(const Map& map,
                                         MapPosition start,
                                         MapPosition goal) {
    // 起点或终点非法时直接失败。终点必须可行走。
    if (!map.inBounds(start) || !map.inBounds(goal) || !map.canWalkAt(goal)) {
        return {};
    }

    // open 保存待探索节点，优先取 fScore 最小者。
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
    // cameFrom 用于最终从终点反向重建路径。
    std::unordered_map<MapPosition, MapPosition> cameFrom;
    // gScore 是从起点走到某节点的真实代价。
    std::unordered_map<MapPosition, int> gScore;

    gScore[start] = 0;
    open.push({start, start.manhattanDistanceTo(goal)});

    while (!open.empty()) {
        MapPosition current = open.top().position;
        open.pop();

        if (current == goal) {
            // 到达终点后反向回溯，最后再翻转成 start -> goal。
            std::vector<MapPosition> path;
            path.push_back(current);
            while (cameFrom.find(current) != cameFrom.end()) {
                current = cameFrom[current];
                path.push_back(current);
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        for (MapPosition next : map.neighbors4(current)) {
            // 当前版本四方向移动代价固定为 1。
            int tentative = gScore[current] + 1;
            auto it = gScore.find(next);
            if (it == gScore.end() || tentative < it->second) {
                // 找到更短路线时更新父节点和分数。
                cameFrom[next] = current;
                gScore[next] = tentative;
                int fScore = tentative + next.manhattanDistanceTo(goal);
                open.push({next, fScore});
            }
        }
    }

    return {};
}

} // namespace game::core
