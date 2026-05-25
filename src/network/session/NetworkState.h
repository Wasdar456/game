#ifndef NETWORK_STATE_H
#define NETWORK_STATE_H

#include <QObject>

namespace game {
namespace network {

// ═══════════════════════════════════════════════════════════════
// 网络连接状态
// ═══════════════════════════════════════════════════════════════
enum class ConnectionState {
    Disconnected,    // 未连接
    Connecting,      // 正在连接
    Connected,       // 已连接
    Negotiating,     // 协商中（版本验证等）
    Ready,           // 准备就绪（可以开始游戏）
    Reconnecting,   // 断线重连中
    Error           // 发生错误
};

// ═══════════════════════════════════════════════════════════════
// 房间角色
// ═══════════════════════════════════════════════════════════════
enum class RoomRole {
    None,       // 无角色
    Host,       // 房主（服务端）
    Client      // 客户端（加入者）
};

// ═══════════════════════════════════════════════════════════════
// 获取状态名称（调试用）
// ═══════════════════════════════════════════════════════════════
inline const char* connectionStateName(ConnectionState state) {
    switch (state) {
        case ConnectionState::Disconnected: return "Disconnected";
        case ConnectionState::Connecting:     return "Connecting";
        case ConnectionState::Connected:      return "Connected";
        case ConnectionState::Negotiating:    return "Negotiating";
        case ConnectionState::Ready:          return "Ready";
        case ConnectionState::Reconnecting: return "Reconnecting";
        case ConnectionState::Error:          return "Error";
        default:                              return "Unknown";
    }
}

inline const char* roomRoleName(RoomRole role) {
    switch (role) {
        case RoomRole::None:   return "None";
        case RoomRole::Host:   return "Host";
        case RoomRole::Client: return "Client";
        default:               return "Unknown";
    }
}

} // namespace network
} // namespace game

#endif // NETWORK_STATE_H
