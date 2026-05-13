#ifndef PACKET_H
#define PACKET_H

#include "ProtocolDef.h"
#include <QtGlobal>

namespace game {
namespace network {

// ═══════════════════════════════════════════════════════════════
// 包结构：固定 3 字节包头 + 变长包体
// ═══════════════════════════════════════════════════════════════
#pragma pack(push, 1)
struct PacketHeader {
    quint8  msgType;    // 消息类型（1字节）
    quint16 bodyLen;    // 包体长度（2字节，大端序）
};
#pragma pack(pop)

// 完整包 = PacketHeader(3字节) + body(bodyLen字节)

// ═══════════════════════════════════════════════════════════════
// 包大小常量
// ═══════════════════════════════════════════════════════════════
constexpr size_t PACKET_HEADER_SIZE = sizeof(PacketHeader);  // = 3

// ═══════════════════════════════════════════════════════════════
// 最大包体长度（防止恶意数据）
// ═══════════════════════════════════════════════════════════════
constexpr size_t MAX_PACKET_BODY_SIZE = 65535;  // 2^16 - 1

} // namespace network
} // namespace game

#endif // PACKET_H
