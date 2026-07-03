#ifndef PROTOCOL_DEF_H
#define PROTOCOL_DEF_H

#include <cstdint>
#include <QtGlobal>

namespace game {
namespace network {

// ═══════════════════════════════════════════════════════════════
// 消息类型枚举（双方必须保持一致，改动需同步更新）
// ═══════════════════════════════════════════════════════════════
enum class MsgType : uint8_t {
    // ─── 大厅/握手阶段 ───
    JOIN_ROOM      = 0x01,   // Client 加入房间（携带昵称）
    JOIN_ACK       = 0x02,   // Host 确认加入（携带房主昵称）
    PLAYER_READY   = 0x03,   // 玩家按下"准备"按钮
    PLAYER_UNREADY = 0x04,   // 玩家取消准备
    GAME_START     = 0x05,   // 双方都准备好，Host 广播开始
    SYNC_SEED      = 0x06,   // 随机数种子同步（GAME_START 携带）
    MAP_SELECTION  = 0x07,   // Host 广播当前大厅地图选择

    // ─── 部署阶段 ───
    DEPLOY         = 0x10,   // 部署单位
    RECALL_UNIT    = 0x11,   // 撤回单位
    DEPLOYMENT_END = 0x12,   // 部署阶段结束
    DEPLOYMENT_START = 0x13, // 双方部署结束，Host 通知进入战斗

    // ─── 战斗阶段 ───
    MOVE_UNIT      = 0x20,   // 移动单位
    UPGRADE_UNIT   = 0x21,   // 升级单位
    WAVE_START     = 0x22,   // 波次开始
    WAVE_COMPLETE  = 0x23,   // 波次完成
    CLASH_RESULT   = 0x24,   // 拼点结果
    WAVE_CLEAR     = 0x25,   // 本端波次怪物已清空，等待双方统一回部署

    // ─── 状态同步 ───
    UNIT_HP_SYNC   = 0x30,   // 单位血量同步
    CORE_HP_SYNC   = 0x31,   // 核心血量同步
    RESOURCE_SYNC  = 0x32,   // 资源数量同步
    BATTLE_STATE   = 0x33,   // Host 权威战斗快照

    // ─── 特殊事件 ───
    MONSTER_KILLED = 0x40,   // 怪物击杀
    UNIT_DESTROYED = 0x41,   // 单位销毁
    GAME_OVER      = 0x50,   // 游戏结束

    // ─── 广播+双方确认机制（RoundManager 使用）───
    ROUND_VALUE    = 0x60,   // Host 向双方广播一个值
    ROUND_ACK      = 0x61,   // 一方确认处理完毕
    ROUND_COMPLETE = 0x62,   // 双方都确认，本轮完成

    // ─── 系统 ───
    PING           = 0xFE,   // 心跳请求（发PING的一方用）
    PONG           = 0xFD,   // 心跳响应（收到PING后回PONG）
    DISCONNECT     = 0xFF    // 主动断开
};

// ═══════════════════════════════════════════════════════════════
// 获取消息名称（用于调试日志）
// ═══════════════════════════════════════════════════════════════
inline const char* msgTypeName(MsgType type) {
    switch (type) {
        case MsgType::JOIN_ROOM:      return "JOIN_ROOM";
        case MsgType::JOIN_ACK:       return "JOIN_ACK";
        case MsgType::PLAYER_READY:   return "PLAYER_READY";
        case MsgType::PLAYER_UNREADY: return "PLAYER_UNREADY";
        case MsgType::GAME_START:     return "GAME_START";
        case MsgType::SYNC_SEED:      return "SYNC_SEED";
        case MsgType::MAP_SELECTION:  return "MAP_SELECTION";
        case MsgType::DEPLOY:         return "DEPLOY";
        case MsgType::RECALL_UNIT:    return "RECALL_UNIT";
        case MsgType::DEPLOYMENT_END: return "DEPLOYMENT_END";
        case MsgType::DEPLOYMENT_START: return "DEPLOYMENT_START";
        case MsgType::MOVE_UNIT:      return "MOVE_UNIT";
        case MsgType::UPGRADE_UNIT:   return "UPGRADE_UNIT";
        case MsgType::WAVE_START:     return "WAVE_START";
        case MsgType::WAVE_COMPLETE:  return "WAVE_COMPLETE";
        case MsgType::CLASH_RESULT:   return "CLASH_RESULT";
        case MsgType::WAVE_CLEAR:     return "WAVE_CLEAR";
        case MsgType::UNIT_HP_SYNC:   return "UNIT_HP_SYNC";
        case MsgType::CORE_HP_SYNC:   return "CORE_HP_SYNC";
        case MsgType::RESOURCE_SYNC:  return "RESOURCE_SYNC";
        case MsgType::BATTLE_STATE:   return "BATTLE_STATE";
        case MsgType::MONSTER_KILLED: return "MONSTER_KILLED";
        case MsgType::UNIT_DESTROYED: return "UNIT_DESTROYED";
        case MsgType::GAME_OVER:      return "GAME_OVER";
        case MsgType::ROUND_VALUE:    return "ROUND_VALUE";
        case MsgType::ROUND_ACK:      return "ROUND_ACK";
        case MsgType::ROUND_COMPLETE: return "ROUND_COMPLETE";
        case MsgType::PING:           return "PING";
        case MsgType::PONG:           return "PONG";
        case MsgType::DISCONNECT:     return "DISCONNECT";
        default:                      return "UNKNOWN";
    }
}


// ═══════════════════════════════════════════════════════════════
// 和 PVP 组对接用的数据结构
//
// 约定：
//   - 怪物类型编号 与 ObjectType 枚举对应，见下方 MonsterKind
//   - 卡牌（塔）类型编号 与 CardKind 对应
//   - 波次数据不传具体怪物列表，双方用同一 RNG 种子独立生成，
//     只传波号 waveId 以保持同步触发。
//   - 部署消息在"迷雾部署"阶段不互通，DEPLOYMENT_END 后才同步。
// ═══════════════════════════════════════════════════════════════

// ─── 怪物种类编号（与 ObjectType 枚举对齐，紧凑编码传输用）───
// 对应关系：
//   0 = MONSTER_RES_BASIC    普通资源怪
//   1 = MONSTER_RES_FAST     快速资源怪
//   2 = MONSTER_RES_TANK     肥硕资源怪
//   3 = MONSTER_ATK_NORMAL   普通攻击怪
//   4 = MONSTER_ATK_TANK     坦克攻击怪
//   5 = MONSTER_ATK_FAST     快速攻击怪
//   6 = MONSTER_ATK_SAPPER   工兵/拆台
//   7 = MONSTER_ATK_BERSERK  狂暴怪
//   8 = MONSTER_ATK_RANGED   远程怪
//   9 = MONSTER_ATK_REGEN    再生怪
enum class MonsterKind : quint8 {
    RES_BASIC   = 0,
    RES_FAST    = 1,
    RES_TANK    = 2,
    ATK_NORMAL  = 3,
    ATK_TANK    = 4,
    ATK_FAST    = 5,
    ATK_SAPPER  = 6,
    ATK_BERSERK = 7,
    ATK_RANGED  = 8,
    ATK_REGEN   = 9
};

// ─── 卡牌（塔）种类编号 ───
//   0 = CARD_ATTACK    攻击型单位
//   1 = CARD_PRODUCE   生产型单位
//   2 = CARD_HEAL      治疗型单位
enum class CardKind : quint8 {
    ATTACK  = 0,
    SNIPER  = 1,
    AOE     = 2,
    SPECIALIST = 3,
    PRODUCE = 4,
    ARSENAL = 5,
    HEAL    = 6,
    HEAVY_MEDIC = 7,
    ATTACK2 = 8,
    HEAL2 = 9
};

// ─── WAVE_START 消息 payload（4 字节）───
// Host 在每波开始时广播，双方用相同 RNG 种子+waveId 独立生成怪物序列
// 因此不需要传怪物数组，只传波号即可。
// layout: [ waveId(1) | reserved(3) ]
struct WaveStartPayload {
    quint8  waveId;       // 当前波次编号（从 1 开始）
    quint8  reserved[3];  // 保留，填 0
};

// ─── DEPLOY 消息 payload（7 字节）───
// 迷雾阶段双方各自部署，DEPLOYMENT_END 后 Host 统一同步。
// 实时部署阶段（若允许看到对方）也用此包。
// layout: [ cardKind(1) | row(2) | col(2) | unitId(2) ], big-endian.
struct DeployPayload {
    quint8 cardKind;  // CardKind 枚举值
    qint16 row;       // 部署行（0-based）
    qint16 col;       // 部署列（0-based）
    quint16 unitId;   // Host 分配的单位 ID（迷雾同步时用，实时部署可填 0）
};

// ─── UPGRADE_UNIT 消息 payload（3 字节）───
// 触发条件：玩家消耗资源，升级已部署的某个 Card
// layout: [ unitId(2) | targetLevel(1) ], big-endian.
struct UpgradePayload {
    quint16 unitId;     // 要升级的单位 ID（与部署时分配的 unitId 对应）
    quint8 targetLevel; // 目标等级（当前 Card 支持 1~3 级）
};

// ─── MOVE_UNIT 消息 payload（6 字节）───
// layout: [ unitId(2) | row(2) | col(2) ], big-endian signed/unsigned integers.
struct MovePayload {
    quint16 unitId;
    qint16 row;
    qint16 col;
};

// ─── RECALL_UNIT 消息 payload（2 字节）───
// layout: [ unitId(2) ], big-endian.
struct RecallPayload {
    quint16 unitId;  // 要撤回的单位 ID
};

// ─── CORE_HP_SYNC 消息 payload（4 字节）───
// Host 广播己方（或对方）基地当前血量
// layout: [ hp(4, big-endian) ]
struct CoreHpPayload {
    quint32 hp;  // PlayerState::baseHealth 当前值
};

// ─── RESOURCE_SYNC 消息 payload（4 字节）───
// 广播己方当前资源量（金/币）
// layout: [ amount(4, big-endian) ]
struct ResourcePayload {
    quint32 amount;  // PlayerState::currentResources 当前值
};

} // namespace network
} // namespace game

#endif // PROTOCOL_DEF_H
