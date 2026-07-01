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

    // ═══════════════════════════════════════════════════════════
    // 序列化业务结构体
    // ═══════════════════════════════════════════════════════════

    // WaveStartPayload → 4 字节
    static QByteArray serialize(const WaveStartPayload& p) {
        QByteArray d;
        d.push_back(static_cast<char>(p.waveId));
        d.append(3, '\0');  // reserved[3]
        return d;
    }

    // DeployPayload → 7 字节
    static QByteArray serialize(const DeployPayload& p) {
        QByteArray d;
        d.push_back(static_cast<char>(p.cardKind));
        d.append(encodeUint16(static_cast<quint16>(p.row)));
        d.append(encodeUint16(static_cast<quint16>(p.col)));
        d.append(encodeUint16(p.unitId));
        return d;
    }

    // UpgradePayload → 3 字节
    static QByteArray serialize(const UpgradePayload& p) {
        QByteArray d;
        d.append(encodeUint16(p.unitId));
        d.push_back(static_cast<char>(p.targetLevel));
        return d;
    }

    // MovePayload → 6 字节
    static QByteArray serialize(const MovePayload& p) {
        QByteArray d;
        d.append(encodeUint16(p.unitId));
        d.append(encodeUint16(static_cast<quint16>(p.row)));
        d.append(encodeUint16(static_cast<quint16>(p.col)));
        return d;
    }

    // RecallPayload → 2 字节
    static QByteArray serialize(const RecallPayload& p) {
        return encodeUint16(p.unitId);
    }

    // CoreHpPayload → 4 字节
    static QByteArray serialize(const CoreHpPayload& p) {
        return encodeUint32(p.hp);
    }

    // ResourcePayload → 4 字节
    static QByteArray serialize(const ResourcePayload& p) {
        return encodeUint32(p.amount);
    }

    // ─── 大厅消息快捷序列化 ───

    // JOIN_ROOM body: nickname（字符串）
    static QByteArray buildJoinRoom(const QString& nickname) {
        return encodeString(nickname);
    }

    // JOIN_ACK body: hostname（字符串）
    static QByteArray buildJoinAck(const QString& hostname) {
        return encodeString(hostname);
    }

    // GAME_START body: seed（4字节大端序）
    static QByteArray buildGameStart(quint32 seed) {
        return encodeUint32(seed);
    }

    // SYNC_SEED body: seed（4字节大端序）
    static QByteArray buildSyncSeed(quint32 seed) {
        return encodeUint32(seed);
    }
};

} // namespace network
} // namespace game

#endif // SERIALIZER_H
