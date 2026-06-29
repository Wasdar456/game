#ifndef VISION_MANAGER_H
#define VISION_MANAGER_H

#include <set>
#include <vector>
#include <memory>
#include "core/map/MapPosition.h"
#include "core/map/Map.h"
#include "core/units/Card.h"

namespace game::core {

/**
 * @class VisionManager
 * @brief 视野管理器 —— 管理双方的视野范围
 *
 * 视野规则：
 * 1. 核心周围 3x3 区域默认可见
 * 2. 已部署单位的攻击范围 + 1 格为视野范围
 * 3. 高台单位视野 + 1
 */
class VisionManager {
public:
    VisionManager();
    void clear();

    /**
     * @brief 初始化默认视野（核心周围）
     * @param coreAPos A方核心位置
     * @param coreBPos B方核心位置
     */
    void initDefaultVision(MapPosition coreAPos, MapPosition coreBPos);

    /**
     * @brief 更新视野范围
     * @param map 地图引用
     * @param cardsA A方单位列表
     * @param cardsB B方单位列表
     */
    void updateVision(const Map& map,
                      const std::vector<std::shared_ptr<Card>>& cardsA,
                      const std::vector<std::shared_ptr<Card>>& cardsB);

    /**
     * @brief 检查位置是否在A方视野内
     */
    bool isInVisionA(MapPosition pos) const;

    /**
     * @brief 检查位置是否在B方视野内
     */
    bool isInVisionB(MapPosition pos) const;

    /**
     * @brief 获取A方可部署区域（基于视野）
     */
    std::vector<MapPosition> getDeployableCellsA(const Map& map) const;

    /**
     * @brief 获取B方可部署区域（基于视野）
     */
    std::vector<MapPosition> getDeployableCellsB(const Map& map) const;
    const std::set<MapPosition>& visionCellsA() const { return visionA_; }
    const std::set<MapPosition>& visionCellsB() const { return visionB_; }
    void setVisionBlocks(std::vector<MapPosition> blocks);

private:
    std::set<MapPosition> visionA_;  // A方视野
    std::set<MapPosition> visionB_;  // B方视野
    std::set<MapPosition> visionBlocks_;
    MapPosition coreAPos_;
    MapPosition coreBPos_;
    bool hasCoreVision_ = false;

    /**
     * @brief 添加单位周围的视野
     */
    void addVisionAround(std::set<MapPosition>& vision,
                         MapPosition center,
                         int range,
                         const Map& map);
    void addCoreVision(std::set<MapPosition>& vision, MapPosition center);
};

} // namespace game::core

#endif // VISION_MANAGER_H
