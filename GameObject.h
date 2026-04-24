#pragma once
#include <iostream>

// ==========================================
// 核心枚举：ObjectType (对象类型)
// 作用：用于在不使用 dynamic_cast 的情况下，快速判断当前对象是什么。
// 这对于你的索敌逻辑 (priorityList) 和碰撞判定至关重要。
// ==========================================
enum class ObjectType {
    NONE,
    
    // --- 玩家部署单位 (防御塔/卡牌) ---
    CARD_ATTACK,      // 攻击型单位 (如：AOE炮塔，负责输出)
    CARD_PRODUCE,     // 生产型单位 (如：采矿工/兵工厂，负责产出资源)
    CARD_HEAL,        // 治疗型单位 (如：医生，负责回血)
    
    // --- 资源型怪物 (公共区域生成，高收益，无害) ---
    MONSTER_RES_BASIC,  // 普通小金猪 (属性均衡，掉落中等资源)
    MONSTER_RES_FAST,   // 盗宝飞贼 (极高移速，低血量，容易逃跑)
    MONSTER_RES_TANK,   // 移动金库 (极慢移速，极高血量，掉落海量资源)

    // --- 攻击型怪物 (直奔大本营，造成伤害) ---
    MONSTER_ATK_NORMAL, // 普通步兵 (基础属性)
    MONSTER_ATK_TANK,   // 肉盾怪 (高血低速，用于吸收防御塔火力)
    MONSTER_ATK_FAST,   // 猎犬怪 (低血高速，极易突破防线漏怪)
    MONSTER_ATK_SAPPER, // 拆迁怪/工兵 (特殊索敌：优先攻击高台上的单位)
    MONSTER_ATK_BERSERK,// 狂暴怪 (特殊被动：每次受击增加移速和攻速)
    MONSTER_ATK_RANGED, // 远程怪 (自带射程，可隔空攻击防御塔)
    MONSTER_ATK_REGEN   // 自愈怪 (自带被动：随时间自动恢复血量)
};

// ==========================================
// 基类：GameObject
// 作用：所有存在于地图上的动态/静态对象的绝对根节点。
// ==========================================
class GameObject {
protected:
    int id;             // 全局唯一标识符 (方便网络同步时精确索敌)
    int posX;           // 所在的地图网格 X 坐标 (列)
    int posY;           // 所在的地图网格 Y 坐标 (行)
    ObjectType type;    // 对象类型标识

public:
    GameObject(int id, int x, int y, ObjectType t) 
        : id(id), posX(x), posY(y), type(t) {}
    
    virtual ~GameObject() = default; 

    // [纯虚函数] 核心主循环接口
    // update(): 由场景管理器每帧调用，处理内部逻辑（如移动、冷却、索敌）
    virtual void update() = 0; 
    
    // draw(): 留给 UI 成员 C 调用的渲染接口。
    // 注意：Phase 1 阶段请在这里用 QPainter 画纯色方块占位即可。
    virtual void draw() = 0;   

    // 基础信息 Get 接口
    int getID() const { return id; }
    int getX() const { return posX; }
    int getY() const { return posY; }
    ObjectType getType() const { return type; }
    
    // 基础信息 Set 接口 (例如单位瞬移时更新坐标)
    void setPosition(int x, int y) { posX = x; posY = y; }
};