#include <network/sync/StateValidator.h>
#include <network/protocol/Serializer.h>
#include <network/protocol/Deserializer.h>
#include <QDebug>

namespace game {
namespace network {

// ═══════════════════════════════════════════════════════════════
// 构造函数
// ═══════════════════════════════════════════════════════════════
StateValidator::StateValidator(QObject* parent)
    : QObject(parent)
{}

// ═══════════════════════════════════════════════════════════════
// 更新本地快照
// ═══════════════════════════════════════════════════════════════
void StateValidator::updateLocalSnapshot(const QMap<int, UnitSnapshot>& units) {
    m_localSnapshot = units;
}

// ═══════════════════════════════════════════════════════════════
// 序列化快照
//
// 格式：[2字节数量] + {[4字节unitId][2字节hp][1字节level][2字节row][2字节col]}
// ═══════════════════════════════════════════════════════════════
QByteArray StateValidator::serializeSnapshot(const QMap<int, UnitSnapshot>& units) {
    QByteArray body;

    // [2字节] 单位数量
    body.append(Serializer::encodeUint16(static_cast<quint16>(units.size())));

    for (auto it = units.constBegin(); it != units.constEnd(); ++it) {
        const UnitSnapshot& s = it.value();

        body.append(Serializer::encodeUint32(static_cast<quint32>(s.unitId)));
        body.append(Serializer::encodeUint16(static_cast<quint16>(s.hp)));
        body.append(Serializer::encodeUint8(static_cast<quint8>(s.level)));
        body.append(Serializer::encodeUint16(static_cast<quint16>(s.row)));
        body.append(Serializer::encodeUint16(static_cast<quint16>(s.col)));
    }

    return body;
}

// ═══════════════════════════════════════════════════════════════
// 解析快照
// ═══════════════════════════════════════════════════════════════
QMap<int, UnitSnapshot> StateValidator::parseSnapshot(const QByteArray& body) {
    QMap<int, UnitSnapshot> result;

    if (body.size() < 2) {
        qWarning() << "[StateValidator] 快照数据太短";
        return result;
    }

    Deserializer d(body);

    quint16 count;
    if (!d.decodeUint16(count)) return result;

    for (quint16 i = 0; i < count; ++i) {
        UnitSnapshot s;
        quint32 unitId;
        quint16 hp, row, col;
        quint8 level;

        if (!d.decodeUint32(unitId)) break;
        if (!d.decodeUint16(hp)) break;
        if (!d.decodeUint8(level)) break;
        if (!d.decodeUint16(row)) break;
        if (!d.decodeUint16(col)) break;

        s.unitId = static_cast<int>(unitId);
        s.hp = static_cast<int>(hp);
        s.level = static_cast<int>(level);
        s.row = static_cast<int>(row);
        s.col = static_cast<int>(col);

        result[s.unitId] = s;
    }

    qDebug() << "[StateValidator] 解析到" << result.size() << "个单位状态";
    return result;
}

// ═══════════════════════════════════════════════════════════════
// 比对状态差异
// ═══════════════════════════════════════════════════════════════
QStringList StateValidator::compareWith(const QMap<int, UnitSnapshot>& other) const {
    QStringList differences;

    // 检查本地多出的单位
    for (auto it = m_localSnapshot.constBegin(); it != m_localSnapshot.constEnd(); ++it) {
        if (!other.contains(it.key())) {
            differences.append(QString("本地多出单位 %1").arg(it.key()));
        }
    }

    // 检查对方多出的单位
    for (auto it = other.constBegin(); it != other.constEnd(); ++it) {
        if (!m_localSnapshot.contains(it.key())) {
            differences.append(QString("对方多出单位 %1").arg(it.key()));
        }
    }

    // 检查共同单位的数值差异
    for (auto it = m_localSnapshot.constBegin(); it != m_localSnapshot.constEnd(); ++it) {
        if (other.contains(it.key())) {
            const UnitSnapshot& local = it.value();
            const UnitSnapshot& remote = other[it.key()];

            if (local.hp != remote.hp) {
                differences.append(QString("单位%1 HP: 本地=%2 对方=%3")
                    .arg(it.key()).arg(local.hp).arg(remote.hp));
            }
            if (local.level != remote.level) {
                differences.append(QString("单位%1 等级: 本地=%2 对方=%3")
                    .arg(it.key()).arg(local.level).arg(remote.level));
            }
            if (local.row != remote.row || local.col != remote.col) {
                differences.append(QString("单位%1 位置: 本地=(%2,%3) 对方=(%4,%5)")
                    .arg(it.key()).arg(local.row).arg(local.col)
                    .arg(remote.row).arg(remote.col));
            }
        }
    }

    if (differences.isEmpty()) {
        qDebug() << "[StateValidator] 状态完全一致";
    } else {
        qWarning() << "[StateValidator] 发现状态差异:" << differences.size() << "处";
        for (const QString& diff : differences) {
            qWarning() << "  -" << diff;
        }
    }

    return differences;
}

} // namespace network
} // namespace game
