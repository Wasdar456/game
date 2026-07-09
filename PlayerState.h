#pragma once
#include <iostream>

// ==========================================
// 全局状态管理类：PlayerState (单例模式)
// 作用：统筹全局的金币(资源)、大本营血量等共享数据。
// ==========================================
class PlayerState {
private:
    int currentResources; // 当前拥有的金币/资源总数
    int baseHealth;       // 玩家大本营（核心）的血量

    // 单例模式：私有化构造函数
    PlayerState() : currentResources(100), baseHealth(10) {} 

public:
    // 获取全局唯一的实例
    static PlayerState* getInstance() {
        static PlayerState instance;
        return &instance;
    }

    // 禁用拷贝构造和赋值操作
    PlayerState(const PlayerState&) = delete;
    PlayerState& operator=(const PlayerState&) = delete;

    // ================= 资源(金币)操作接口 =================
    int getResources() const { return currentResources; }
    
    // 增加资源：打怪掉钱 (调用此函数)、生产型单位产钱 (调用此函数)
    void addResource(int amount) {
        currentResources += amount;
        std::cout << "Gained " << amount << " resources. Total: " << currentResources << std::endl;
    }

    // 消耗资源：部署单位、升级单位、移动单位 时调用
    bool consumeResource(int amount) {
        if (currentResources >= amount) {
            currentResources -= amount;
            return true; // 资源足够，扣除成功
        }
        return false;    // 资源不足，操作失败
    }

    // ================= 基地血量操作接口 =================
    int getBaseHealth() const { return baseHealth; }

    // 怪物进家：调用此函数扣除基地血量
    void damageBase(int damage) {
        baseHealth -= damage;
        if (baseHealth <= 0) {
            std::cout << "Game Over! Base destroyed." << std::endl;
            // TODO: 触发游戏失败逻辑
        }
    }
};