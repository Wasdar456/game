#ifndef SERIALIZER_H
#define SERIALIZER_H

#include "Packet.h"
#include "ProtocolDef.h"
#include <QtEndian>
#include <QByteArray>
#include <QDataStream>
#include <QIODevice>

namespace game {
namespace network {

// ═══════════════════════════════════════════════════════════════
// 序列化器：将结构体 → QByteArray
// ═══════════════════════════════════════════════════════════════
class Serializer {
public:
    Serializer() = default;

    // ═══════════════════════════════════════════════════════════
    // 创建完整数据包
    // ═══════════════════════════════════════════════════════════
    static QByteArray buildPacket(MsgType type, const QByteArray& body = {}) {
        QByteArray packet;
        packet.reserve(PACKET_HEADER_SIZE + body.size());

        // 包头
        PacketHeader header;
        header.msgType = static_cast<quint8>(type);
        header.bodyLen = qToBigEndian(static_cast<quint16>(body.size()));

        packet.append(reinterpret_cast<const char*>(&header), PACKET_HEADER_SIZE);
        packet.append(body);

        return packet;
    }

    // ═══════════════════════════════════════════════════════════
    // 快捷方法：直接序列化基础类型
    // ═══════════════════════════════════════════════════════════
    static QByteArray encodeUint8(quint8 value) {
        return QByteArray(1, static_cast<char>(value));
    }

    static QByteArray encodeUint16(quint16 value) {
        QByteArray data(2, '\0');
        qToBigEndian(value, reinterpret_cast<uchar*>(data.data()));
        return data;
    }

    static QByteArray encodeUint32(quint32 value) {
        QByteArray data(4, '\0');
        qToBigEndian(value, reinterpret_cast<uchar*>(data.data()));
        return data;
    }

    static QByteArray encodeInt32(qint32 value) {
        return encodeUint32(static_cast<quint32>(value));
    }

    // ═══════════════════════════════════════════════════════════
    // 序列化字符串（长度前缀 + 内容）
    // ═══════════════════════════════════════════════════════════
    static QByteArray encodeString(const QString& str) {
        QByteArray utf8 = str.toUtf8();
        QByteArray result = encodeUint16(static_cast<quint16>(utf8.size()));
        result.append(utf8);
        return result;
    }
};

} // namespace network
} // namespace game

#endif // SERIALIZER_H
