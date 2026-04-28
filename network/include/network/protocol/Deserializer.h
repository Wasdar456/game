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

private:
    const QByteArray& m_data;
    size_t m_pos;
};

} // namespace network
} // namespace game

#endif // DESERIALIZER_H
