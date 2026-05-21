#ifndef STATE_VALIDATOR_H
#define STATE_VALIDATOR_H

#include "../protocol/ProtocolDef.h"
#include <QObject>
#include <QMap>

namespace game {
namespace network {

// ═══════════════════════════════════════════════════════════════
// 单位状态快照（用于校验）
// ═══════════════════════════════════════════════════════════════
struct UnitSnapshot {
    int unitId;
    int hp;
    int level;
    int row;
    int col;
};

// ═══════════════════════════════════════════════════════════════
// StateValidator - 状态校验器
//
// 定期同步双方关键状态，检测是否有不同步的情况。
// 主要用于调试和防止作弊。
// ═══════════════════════════════════════════════════════════════
class StateValidator : public QObject {
    Q_OBJECT

public:
    explicit StateValidator(QObject* parent = nullptr);

    // ═══════════════════════════════════════════════════════════
    // 更新本地状态快照
    // ═══════════════════════════════════════════════════════════
    void updateLocalSnapshot(const QMap<int, UnitSnapshot>& units);

    // ═══════════════════════════════════════════════════════════
    // 序列化状态快照
    // ═══════════════════════════════════════════════════════════
    static QByteArray serializeSnapshot(const QMap<int, UnitSnapshot>& units);

    // ═══════════════════════════════════════════════════════════
    // 解析收到的状态快照
    // ═══════════════════════════════════════════════════════════
    static QMap<int, UnitSnapshot> parseSnapshot(const QByteArray& body);

    // ═══════════════════════════════════════════════════════════
    // 比对状态（返回差异列表）
    // ═══════════════════════════════════════════════════════════
    QStringList compareWith(const QMap<int, UnitSnapshot>& other) const;

    const QMap<int, UnitSnapshot>& localSnapshot() const { return m_localSnapshot; }

signals:
    void stateMismatch(const QStringList& differences);

private:
    QMap<int, UnitSnapshot> m_localSnapshot;
};

} // namespace network
} // namespace game

#endif // STATE_VALIDATOR_H
