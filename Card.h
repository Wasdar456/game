#pragma once
#include "Entity.h"
#include <vector>


// ==========================================
// 基类：Card (继承自 Entity)
// 作用：玩家部署的防御塔/单位。包含了技能、瞬移和优先级的核心机制。
// ==========================================
class Card : public Entity {
protected:
    int level;              // 当前等级 (1~3，通过消耗全局资源升级)
    int attackRange;        // 基础攻击范围 (单位：格)
    
    // --- 移动与网络机制 ---
    int moveLimit;          // 瞬移限制 (Chess-like Move)：单次操作能移动的最大格数
    
    // --- 技能与冷却机制 (Timer) ---
    int skillCooldown;      // 技能所需的冷却时间 (以帧或秒计)
    int currentCooldown;    // 当前已积攒的冷却进度
    
    // --- 索敌机制 (Priority) ---
    // 极其重要：定义该单位攻击目标的优先级排序。
    // 例如某塔偏好打资源怪，则将 MONSTER_RES_BASIC 放在 priorityList 队首。
    std::vector<ObjectType> priorityList; 

public:
    Card(int id, int x, int y, ObjectType t, int max_hp, int atk, int range, int limit, int cd);
    virtual ~Card() = default;

    // ==========================================
    // 纯虚函数区 (必须由 AttackUnit/ProduceUnit 等派生类重写)
    // ==========================================
    
    // 自动技能：本项目不设手动释放，冷却完毕且有目标时自动触发
    virtual void autoSkill() = 0;

    // 瞬移逻辑：传入目标坐标，结合 MapGrid 判断是否可部署且未被占用
    virtual bool tryTeleport(int targetX, int targetY) = 0;

    // ==========================================
    // 业务逻辑接口 (具体实现在 Card.cpp 中)
    // ==========================================
    
    // 索敌逻辑：传入当前射程内的敌方实体指针，支持高台射程加成，返回最高权重的目标
    virtual Entity* findTarget(const std::vector<Entity*>& targets);

    // ==========================================
    // 冷却计时器工具函数
    // ==========================================
    
    // 冷却计时器推进 (通常在派生类的 update() 中每帧调用)
    void tickCooldown();
    
    // 查询技能是否就绪
    bool isSkillReady() const;
    
    // 释放技能后重置冷却进度
    void resetCooldown();
};