#ifndef DESERIALIZER_H
#define DESERIALIZER_H

#include "Packet.h"
#include "ProtocolDef.h"
#include <QtEndian>
#include <QByteArray>
#include <QString>

namespace game {
namespace network {

// ═══════════════════════════════════════════════════════════════
// 反序列化器：从 QByteArray → 结构体
// ═══════════════════════════════════════════════════════════════
class Deserializer {
public:
    explicit Deserializer(const QByteArray& data)
        : m_data(data), m_pos(0) {}

    // ═══════════════════════════════════════════════════════════
    // 检查剩余数据是否足够
    // ═══════════════════════════════════════════════════════════
    bool hasData(size_t bytes) const {
        return (m_pos + bytes) <= static_cast<size_t>(m_data.size());
    }

    size_t remaining() const {
        return m_data.size() > m_pos ? m_data.size() - m_pos : 0;
    }

    // ═══════════════════════════════════════════════════════════
    // 读取基础类型（大端序 → 主机字节序）
    // ═══════════════════════════════════════════════════════════
    bool decodeUint8(quint8& out) {
        if (!hasData(1)) return false;
        out = static_cast<quint8>(m_data[m_pos]);
        ++m_pos;
        return true;
    }

    bool decodeUint16(quint16& out) {
        if (!hasData(2)) return false;
        out = qFromBigEndian<quint16>(
            reinterpret_cast<const uchar*>(m_data.constData() + m_pos));
        m_pos += 2;
        return true;
    }

    bool decodeUint32(quint32& out) {
        if (!hasData(4)) return false;
        out = qFromBigEndian<quint32>(
            reinterpret_cast<const uchar*>(m_data.constData() + m_pos));
        m_pos += 4;
        return true;
    }

    bool decodeInt32(qint32& out) {
        quint32 tmp;
        if (!decodeUint32(tmp)) return false;
        out = static_cast<qint32>(tmp);
        return true;
    }

    // ═══════════════════════════════════════════════════════════
    // 读取字符串（长度前缀 + UTF-8 内容）
    // ═══════════════════════════════════════════════════════════
    bool decodeString(QString& out) {
        quint16 len;
        if (!decodeUint16(len)) return false;
        if (!hasData(len)) return false;

        out = QString::fromUtf8(
            m_data.constData() + m_pos, len);
        m_pos += len;
        return true;
    }

    // ═══════════════════════════════════════════════════════════
    // 从原始包数据中解析包头
    // ═══════════════════════════════════════════════════════════
    static bool parseHeader(const QByteArray& data, PacketHeader& out) {
        if (data.size() < PACKET_HEADER_SIZE) return false;
        memcpy(&out, data.constData(), PACKET_HEADER_SIZE);
        out.bodyLen = qFromBigEndian(out.bodyLen);
        return true;
    }

    // ═══════════════════════════════════════════════════════════
    // 反序列化业务结构体（失败返回 false，数据不完整自动截断）
    // ═══════════════════════════════════════════════════════════

    // WaveStartPayload → 4 字节
    bool decode(WaveStartPayload& out) {
        if (!hasData(4)) return false;
        out.waveId = static_cast<quint8>(m_data[m_pos]);
        std::memcpy(out.reserved, m_data.constData() + m_pos + 1, 3);
        m_pos += 4;
        return true;
    }

    // DeployPayload → 4 字节
    bool decode(DeployPayload& out) {
        if (!hasData(4)) return false;
        out.cardKind = static_cast<quint8>(m_data[m_pos]);
        out.row      = static_cast<quint8>(m_data[m_pos + 1]);
        out.col      = static_cast<quint8>(m_data[m_pos + 2]);
        out.unitId   = static_cast<quint8>(m_data[m_pos + 3]);
        m_pos += 4;
        return true;
    }

    // UpgradePayload → 2 字节
    bool decode(UpgradePayload& out) {
        if (!hasData(2)) return false;
        out.unitId      = static_cast<quint8>(m_data[m_pos]);
        out.targetLevel = static_cast<quint8>(m_data[m_pos + 1]);
        m_pos += 2;
        return true;
    }

    // RecallPayload → 1 字节
    bool decode(RecallPayload& out) {
        return decodeUint8(out.unitId);
    }

    // CoreHpPayload → 4 字节
    bool decode(CoreHpPayload& out) {
        return decodeUint32(out.hp);
    }

    // ResourcePayload → 4 字节
    bool decode(ResourcePayload& out) {
        return decodeUint32(out.amount);
    }

    // ─── 大厅消息快捷反序列化 ───

    // JOIN_ROOM body: nickname
    bool decodeJoinRoom(QString& nickname) { return decodeString(nickname); }

    // JOIN_ACK body: hostname
    bool decodeJoinAck(QString& hostname) { return decodeString(hostname); }

    // GAME_START / SYNC_SEED body: seed
    bool decodeGameStart(quint32& seed) { return decodeUint32(seed); }
    bool decodeSyncSeed(quint32& seed) { return decodeUint32(seed); }

private:
    const QByteArray& m_data;
    size_t m_pos;
};

} // namespace network
} // namespace game

#endif // DESERIALIZER_H
