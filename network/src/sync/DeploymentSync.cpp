#include <network/sync/DeploymentSync.h>
#include <network/protocol/Serializer.h>
#include <network/protocol/Deserializer.h>
#include <QDebug>

namespace game {
namespace network {

DeploymentSync::DeploymentSync(QObject* parent)
    : QObject(parent)
    , m_nextUnitId(1)
{}

// ═══════════════════════════════════════════════════════════════
// 添加本地部署
// ═══════════════════════════════════════════════════════════════
void DeploymentSync::addLocalDeploy(const DeployInfo& info) {
    DeployInfo deploy = info;
    deploy.unitId = m_nextUnitId++;
    m_localDeploys.append(deploy);

    qDebug() << "[DeploymentSync] 添加部署 unitId=" << deploy.unitId
             << "cardId=" << deploy.cardId
             << "pos=(" << deploy.gridRow << "," << deploy.gridCol << ")";

    emit localDeployAdded(deploy);
}

// ═══════════════════════════════════════════════════════════════
// 移除本地部署
// ═══════════════════════════════════════════════════════════════
void DeploymentSync::removeLocalDeploy(int unitId) {
    for (int i = 0; i < m_localDeploys.size(); ++i) {
        if (m_localDeploys[i].unitId == unitId) {
            m_localDeploys.removeAt(i);
            qDebug() << "[DeploymentSync] 移除部署 unitId=" << unitId;
            emit localDeployRemoved(unitId);
            return;
        }
    }
    qWarning() << "[DeploymentSync] 找不到要移除的部署 unitId=" << unitId;
}

// ═══════════════════════════════════════════════════════════════
// 清空本地部署
// ═══════════════════════════════════════════════════════════════
void DeploymentSync::clearLocalDeploys() {
    m_localDeploys.clear();
    m_nextUnitId = 1;
    qDebug() << "[DeploymentSync] 清空所有部署";
}

// ═══════════════════════════════════════════════════════════════
// 序列化部署信息包
// ═══════════════════════════════════════════════════════════════
QByteArray DeploymentSync::buildDeploySyncPacket(const QVector<DeployInfo>& deploys) {
    QByteArray body;

    // [2字节] 部署数量
    body.append(Serializer::encodeUint16(static_cast<quint16>(deploys.size())));

    for (const DeployInfo& deploy : deploys) {
        // [4字节] cardId
        body.append(Serializer::encodeUint32(static_cast<quint32>(deploy.cardId)));
        // [4字节] unitId
        body.append(Serializer::encodeUint32(static_cast<quint32>(deploy.unitId)));
        // [2字节] gridRow
        body.append(Serializer::encodeUint16(static_cast<quint16>(deploy.gridRow)));
        // [2字节] gridCol
        body.append(Serializer::encodeUint16(static_cast<quint16>(deploy.gridCol)));
    }

    return body;
}

// ═══════════════════════════════════════════════════════════════
// 解析部署信息包
// ═══════════════════════════════════════════════════════════════
QVector<DeployInfo> DeploymentSync::parseDeploySyncPacket(const QByteArray& body) {
    QVector<DeployInfo> result;

    if (body.size() < 2) {
        qWarning() << "[DeploymentSync] 部署数据太短";
        return result;
    }

    Deserializer d(body);

    quint16 count;
    if (!d.decodeUint16(count)) return result;

    result.reserve(count);

    for (quint16 i = 0; i < count; ++i) {
        DeployInfo info;

        quint32 cardId, unitId;
        quint16 row, col;

        if (!d.decodeUint32(cardId)) break;
        if (!d.decodeUint32(unitId)) break;
        if (!d.decodeUint16(row)) break;
        if (!d.decodeUint16(col)) break;

        info.cardId = static_cast<int>(cardId);
        info.unitId = static_cast<int>(unitId);
        info.gridRow = static_cast<int>(row);
        info.gridCol = static_cast<int>(col);

        result.append(info);
    }

    qDebug() << "[DeploymentSync] 解析到" << result.size() << "个部署";
    return result;
}

} // namespace network
} // namespace game
#include "DeploymentSync.moc"
