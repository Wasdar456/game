#ifndef DEBUG_CONFIG_H
#define DEBUG_CONFIG_H

namespace game::core {

/**
 * @brief 调试配置 —— 集中管理所有调试开关
 *
 * 使用方法：
 *   1. 修改下面的常量值
 *   2. 重新编译运行
 *
 * 发布版本前记得把 DEBUG_ENABLED 设为 false
 */
struct DebugConfig {
    // ===== 总开关 =====
    static constexpr bool DEBUG_ENABLED = true;

    // ===== 怪物生成配置 =====
    /// 每波怪物数量覆盖（0 = 使用 seed + waveId 的难度预算生成）
    static constexpr int MONSTERS_PER_WAVE = 0;

    // ===== 攻击调试 =====
    /// 是否打印攻击日志
    static constexpr bool LOG_ATTACK = true;

    /// 是否打印技能系统状态（每60帧）
    static constexpr bool LOG_SKILL_SYSTEM = true;

    // ===== UI 调试 =====
    /// 是否显示网格坐标
    static constexpr bool SHOW_GRID_COORDS = false;

    /// 是否显示攻击范围
    static constexpr bool SHOW_ATTACK_RANGE = true;
};

} // namespace game::core

#endif // DEBUG_CONFIG_H
