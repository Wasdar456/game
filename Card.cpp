#include "Card.h"
#include "MapGrid.h"
#include <algorithm>
#include <cmath>
#include "PlayerState.h"

// 引用外部地图数组 (假设由成员 B 在别处定义，用于查询地形)
extern MapGrid* g_Map[20][30]; 

// 构造函数实现
Card::Card(int id, int x, int y, ObjectType t, int max_hp, int atk, int range, int limit, int cd)
    : Entity(id, x, y, t, max_hp, atk), level(1), attackRange(range), 
      moveLimit(limit), skillCooldown(cd), currentCooldown(0) {}

// ==========================================
// [核心逻辑] 索敌排序 (集成高台射程加成)
// ==========================================
Entity* Card::findTarget(const std::vector<Entity*>& targets) {
    std::vector<Entity*> validTargets;
    
    // --- 机制 1：高台射程加成 ---
    // 若单位站位 heightValue 为 1 (高台)，其有效射程 +1
    int effectiveRange = attackRange;
    if (g_Map[posY][posX] && g_Map[posY][posX]->getHeight() == 1) {
        effectiveRange += 1;
    }

    // 第一步：初步筛选
    for (Entity* target : targets) {
        if (!target || target->isDead()) continue;
        
        // 计算曼哈顿距离
        int dist = std::abs(target->getX() - posX) + std::abs(target->getY() - posY);
        if (dist <= effectiveRange) {
            validTargets.push_back(target);
        }
    }

    if (validTargets.empty()) return nullptr;

    // 第二步：多级权重排序 (确定性计算)
    std::sort(validTargets.begin(), validTargets.end(), [this](Entity* a, Entity* b) {
        auto itA = std::find(priorityList.begin(), priorityList.end(), a->getType());
        auto itB = std::find(priorityList.begin(), priorityList.end(), b->getType());

        // 规则 1：查 priorityList 字典比对优先级
        if (itA != itB) {
            if (itA != priorityList.end() && itB != priorityList.end()) return itA < itB;
            return itA != priorityList.end();
        }
        
        // 规则 2：优先级相同时，比拼距离
        int distA = std::abs(a->getX() - posX) + std::abs(a->getY() - posY);
        int distB = std::abs(b->getX() - posX) + std::abs(b->getY() - posY);
        if (distA != distB) return distA < distB;
        
        // 规则 3：距离相同，比拼血量 (优先补刀残血)
        if (a->getHP() != b->getHP()) return a->getHP() < b->getHP();
        // 规则 4：终极兜底，比较唯一 ID，保证网络联机时状态绝对同步
        return a->getID() < b->getID(); 
    });

    return validTargets.front();
}

// 计时器相关实现
void Card::tickCooldown() { 
    if (currentCooldown < skillCooldown) currentCooldown++; 
}

bool Card::isSkillReady() const { 
    return currentCooldown >= skillCooldown; 
}

void Card::resetCooldown() { 
    currentCooldown = 0; 
}
// ==========================================
// [核心逻辑] 象棋式瞬移 (Chess-like Move)
// ==========================================
bool Card::tryTeleport(int targetX, int targetY) {
    // 1. 距离规则校验：计算曼哈顿距离
    int dist = std::abs(targetX - posX) + std::abs(targetY - posY);
    if (dist > moveLimit) {
        return false; // 超出该单位单次瞬移的最大步长限制 
    }

    // 2. 地图规则校验：目标格必须合法
    if (targetX < 0 || targetX >= 30 || targetY < 0 || targetY >= 20) return false;
    MapGrid* targetGrid = g_Map[targetY][targetX];
    if (!targetGrid) return false;

    // 判定逻辑：目标格必须为"可部署区块"且当前"未被占用" 
    if (targetGrid->getIsOccupied() || 
        targetGrid->getType() == TerrainType::PATH || 
        targetGrid->getType() == TerrainType::NO_DEPLOY) {
        return false; 
    }

    // 3. 经济规则校验：套用项目文档的消耗公式
    // 移动消耗 = 基础消耗 + (移动距离 * 距离系数) 
    int baseCost = 10;      // 设定基础消耗为 10
    int distanceFactor = 5; // 设定距离系数为 5
    int cost = baseCost + (dist * distanceFactor);

    // 调用全局系统扣除资源 [cite: 154]
    if (!PlayerState::getInstance()->consumeResource(cost)) {
        return false; // 资源不足，拒绝移动
    }

    // 4. 物理转移：更新地图网格状态
    if (g_Map[posY][posX]) {
        g_Map[posY][posX]->setOccupied(false, nullptr); // 释放旧格子
    }

    // 更新自身坐标
    posX = targetX;
    posY = targetY;

    // 占领新格子
    targetGrid->setOccupied(true, this); 

    return true; // 瞬移成功！
}