#pragma once
#include "Entity.h"
#include "PlayerState.h" // 必须引入，因为进家需要扣除基地血量
#include <vector>
#include <cmath>         // 引入数学库计算距离

// 寻路节点结构体
struct Point {
    int x, y;
};

// 路线类型枚举
enum class RouteType {
    MAIN_ROUTE,    // 攻击路线：直奔玩家大本营
    RESOURCE_ROUTE // 资源路线：在公共区域徘徊后离开
};

// ==========================================
// 基类：Monster (怪物核心类)
// ==========================================
class Monster : public Entity {
protected:
    float moveSpeed;        
    int dropReward;         
    int coreDamage;         
    RouteType routeType;    
    int attackRange;        
    
    // --- 寻路与物理移动相关 ---
    int pathIndex;                  
    std::vector<Point> currentPath; 
    
    // [核心新增]：物理浮点坐标，用于实现平滑移动
    float exactX;
    float exactY;

    // [核心新增]：逃逸标识，区分“被打死”和“走到终点”
    bool hasEscaped; 

public:
    Monster(int id, int x, int y, ObjectType t, int max_hp, int atk, float speed, int reward, int dmg, RouteType route, int range = 1)
        : Entity(id, x, y, t, max_hp, atk), moveSpeed(speed), dropReward(reward), 
          coreDamage(dmg), routeType(route), attackRange(range), pathIndex(0), 
          exactX(static_cast<float>(x)), exactY(static_cast<float>(y)), hasEscaped(false) {}

    virtual ~Monster() = default;

    // ==========================================
    // [核心逻辑] 通用寻路与移动算法
    // 绝大多数怪物都可以直接复用这个逻辑，不需要在派生类里重写了！
    // ==========================================
    virtual void AStarMove() {
        // 1. 判定是否已经走完了路径
        if (currentPath.empty() || pathIndex >= currentPath.size()) {
            if (!hasEscaped && !isDead()) {
                hasEscaped = true; // 标记为已逃逸
                // 对大本营造成伤害 (如果是资源怪，coreDamage 设定为0，扣0血)
                PlayerState::getInstance()->damageBase(coreDamage); 
                std::cout << "Monster " << id << " reached the end!" << std::endl;
            }
            return;
        }

        // 2. 获取当前要前往的目标节点
        Point targetNode = currentPath[pathIndex];
        
        // 3. 计算方向向量和距离
        float dx = targetNode.x - exactX;
        float dy = targetNode.y - exactY;
        float distance = std::sqrt(dx * dx + dy * dy);

        // 4. 移动判定
        if (distance <= moveSpeed) {
            // 如果距离已经小于一帧的移动步长，说明"踩"到该节点了
            exactX = targetNode.x;
            exactY = targetNode.y;
            setPosition(targetNode.x, targetNode.y); // 同步给 GameObject 的网格坐标
            pathIndex++; // 瞄准下一个节点
        } else {
            // 还没走到，按照方向匀速逼近
            exactX += (dx / distance) * moveSpeed;
            exactY += (dy / distance) * moveSpeed;
            // 将浮点坐标四舍五入，更新到网格坐标上，供防御塔索敌使用
            setPosition(std::round(exactX), std::round(exactY)); 
        }
    }

    // 死亡回调留给具体怪物实现 (或者之后统一在这里调用 PlayerState 增加金币)
    virtual void onDeath() = 0; 
    
    // --- 对外接口 ---
    RouteType getRouteType() const { return routeType; }
    
    void setPath(const std::vector<Point>& newPath) { 
        currentPath = newPath; 
        pathIndex = 0; 
    }

    // 给主循环用的状态查询
    bool getHasEscaped() const { return hasEscaped; }
};