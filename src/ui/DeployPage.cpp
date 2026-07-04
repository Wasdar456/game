/**
 * @file DeployPage.cpp
 * @brief 迷雾部署页面实现
 */

#include "ui/DeployPage.h"
#include "core/data/CardSpecs.h"
#include "ui/MainWindow.h"
#include "ui/AudioManager.h"
#include "ui/PvpMapLayout.h"
#include "network/protocol/BattleStateCodec.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <QLinearGradient>
#include <QMessageBox>
#include <QResizeEvent>
#include <QPixmapCache>
#include <QtEndian>
#include <QtMath>

#include <algorithm>
#include <cmath>

// ========== 网络模块 ==========
#include "network/session/GameServer.h"
#include "network/session/GameClient.h"

namespace {
QString displayCardName(game::core::CardKind kind)
{
    return QString::fromUtf8(game::core::cardName(kind));
}

const QPixmap* unitArtwork(const game::core::UnitSnapshot& unit)
{
    static const QPixmap kiwi(":/images/new_art/unit_kiwi_scout.png");
    static const QPixmap miner(":/images/new_art/unit_miner_pine.png");
    static const QPixmap mango(":/images/new_art/unit_mango_engineer.png");
    static const QPixmap sniper(":/images/new_art/unit_sniper_berry.png");
    static const QPixmap bomber(":/images/new_art/unit_orange_bomber.png");
    static const QPixmap tank(":/images/new_art/unit_berry_tank.png");
    static const QPixmap healer(":/images/new_art/unit_peach_healer.png");
    static const QPixmap defender(":/images/new_art/unit_coco_defender.png");
    static const QPixmap grape(":/images/new_art/unit_grape_blaster.png");
    static const QPixmap papaya(":/images/new_art/unit_papaya_support.png");

    switch (unit.kind) {
    case game::core::CardKind::Attack: return &kiwi;
    case game::core::CardKind::Sniper: return &sniper;
    case game::core::CardKind::Aoe: return &bomber;
    case game::core::CardKind::Specialist: return &tank;
    case game::core::CardKind::Produce: return &miner;
    case game::core::CardKind::Arsenal: return &mango;
    case game::core::CardKind::Heal: return &healer;
    case game::core::CardKind::HeavyMedic: return &defender;
    case game::core::CardKind::Attack2: return &grape;
    case game::core::CardKind::Heal2: return &papaya;
    }
    return nullptr;
}

void drawCachedArtwork(QPainter& painter, const QRectF& target, const QPixmap& source)
{
    const QSize targetSize = target.size().toSize();
    const QString key = QString("deploy-unit-%1-%2x%3")
        .arg(source.cacheKey()).arg(targetSize.width()).arg(targetSize.height());
    QPixmap scaled;
    if (!QPixmapCache::find(key, &scaled)) {
        scaled = source.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPixmapCache::insert(key, scaled);
    }
    const QPointF topLeft(target.center().x() - scaled.width() * 0.5,
                          target.bottom() - scaled.height());
    painter.drawPixmap(topLeft, scaled);
}

QRectF commandRingRect(const QPointF& center, qreal extent, const QSize& viewport)
{
    const qreal height = extent * 3.2;
    const qreal width = height * 574.0 / 640.0;
    const QPointF shiftedCenter = center + QPointF(extent * 0.24, -extent * 0.22);
    QRectF ring(shiftedCenter.x() - width * 0.48,
                shiftedCenter.y() - height * 0.54,
                width,
                height);
    const qreal margin = 6.0;
    if (ring.left() < margin) ring.moveLeft(margin);
    if (ring.right() > viewport.width() - margin) ring.moveRight(viewport.width() - margin);
    if (ring.top() < margin) ring.moveTop(margin);
    if (ring.bottom() > viewport.height() - margin) ring.moveBottom(viewport.height() - margin);
    return ring;
}

QRect commandButtonRect(const QRectF& ring, const QRectF& normalized)
{
    return QRectF(ring.left() + ring.width() * normalized.x(),
                  ring.top() + ring.height() * normalized.y(),
                  ring.width() * normalized.width(),
                  ring.height() * normalized.height()).toRect();
}

void drawHealthBar(QPainter& painter,
                   const QRectF& rect,
                   int currentHealth,
                   int maxHealth,
                   const QColor& healthyColor)
{
    const qreal ratio = std::clamp(currentHealth / qreal(qMax(1, maxHealth)), 0.0, 1.0);
    const QColor fillColor = ratio <= 0.3
                                 ? QColor(206, 63, 45)
                                 : (ratio <= 0.6 ? QColor(226, 160, 48) : healthyColor);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(75, 50, 31, 220), 2));
    painter.setBrush(QColor(64, 47, 34, 225));
    painter.drawRoundedRect(rect, rect.height() / 2.0, rect.height() / 2.0);

    QRectF fillRect = rect.adjusted(2, 2, -2, -2);
    fillRect.setWidth(fillRect.width() * ratio);
    if (fillRect.width() > 0.5) {
        QLinearGradient fill(fillRect.topLeft(), fillRect.bottomLeft());
        fill.setColorAt(0.0, fillColor.lighter(135));
        fill.setColorAt(0.48, fillColor);
        fill.setColorAt(1.0, fillColor.darker(125));
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawRoundedRect(fillRect, fillRect.height() / 2.0, fillRect.height() / 2.0);
    }
    painter.restore();
}

}

// ============================================================================
// DeployView 实现
// ============================================================================

DeployView::DeployView(QWidget *parent)
    : QWidget(parent)
    , m_mode(InteractionMode::NONE)
    , m_selectedCardKind(game::core::CardKind::Attack)
    , m_selectedUnitId(-1)
    , m_hoverRow(-1)
    , m_hoverCol(-1)
    , m_spawnPos(1, 1)
    , m_corePos(10, 1)
    , m_mapRows(game::core::constants::DefaultMapRows)
    , m_mapCols(game::core::constants::DefaultMapCols)
    , m_artworkOverlayMode(false)
    , m_showGrid(true)
    , m_restrictPvpDeployment(false)
    , m_localIsHost(true)
    , m_unitVisualScale(1.0)
    , m_animFrame(0)
    , m_btnUpgrade(nullptr)
    , m_btnMove(nullptr)
    , m_btnRecall(nullptr)
{
    setMapSize(m_mapRows, m_mapCols);
    setMouseTracking(true);

    m_btnUpgrade = new QPushButton("Upgrade", this);
    m_btnMove = new QPushButton("Move", this);
    m_btnRecall = new QPushButton("Recall", this);

    const QString radialStyle =
        "QPushButton {"
        "  background: transparent;"
        "  border: 2px solid transparent; border-radius: 8px;"
        "}"
        "QPushButton:hover { background: rgba(255,245,180,0.18); border-color: rgba(255,235,130,0.85); }"
        "QPushButton:pressed { background: rgba(70,45,20,0.22); }"
        "QPushButton:disabled { background: rgba(35,30,24,0.34); border-color: transparent; }";
    m_btnUpgrade->setStyleSheet(radialStyle);
    m_btnMove->setStyleSheet(radialStyle);
    m_btnRecall->setStyleSheet(radialStyle);
    m_btnUpgrade->setToolTip("Upgrade");
    m_btnMove->setToolTip("Move");
    m_btnRecall->setToolTip("Recall");
    m_btnUpgrade->setCursor(Qt::PointingHandCursor);
    m_btnMove->setCursor(Qt::PointingHandCursor);
    m_btnRecall->setCursor(Qt::PointingHandCursor);
    hideRadialMenu();

    connect(m_btnUpgrade, &QPushButton::clicked, this, [this]() {
        if (m_selectedUnitId > 0) emit signalUpgradeUnit(m_selectedUnitId);
        hideRadialMenu();
        m_mode = InteractionMode::NONE;
    });

    connect(m_btnMove, &QPushButton::clicked, this, [this]() {
        if (m_selectedUnitId > 0) {
            hideRadialMenu();
            m_mode = InteractionMode::MOVING;
            update();
        }
    });

    connect(m_btnRecall, &QPushButton::clicked, this, [this]() {
        if (m_selectedUnitId > 0) emit signalRecallUnit(m_selectedUnitId);
        hideRadialMenu();
        m_mode = InteractionMode::NONE;
        m_selectedUnitId = -1;
        update();
    });

    auto *effectTimer = new QTimer(this);
    effectTimer->setInterval(40);
    connect(effectTimer, &QTimer::timeout, this, [this]() {
        ++m_animFrame;
        for (auto &effect : m_dustEffects) {
            effect.life -= 0.04;
        }
        m_dustEffects.erase(std::remove_if(m_dustEffects.begin(), m_dustEffects.end(),
                                           [](const DustEffect &effect) {
                                               return effect.life <= 0.0;
                                           }),
                            m_dustEffects.end());
        if (!m_dustEffects.isEmpty() || !m_snapshot.units.empty()) {
            update();
        }
    });
    effectTimer->start();
}

void DeployView::setMapSize(int rows, int cols)
{
    m_mapRows = rows;
    m_mapCols = cols;
    if (!m_artworkOverlayMode) {
        this->setFixedSize(cols * CELL_SIZE, rows * CELL_SIZE);
    }
}

void DeployView::setPvpDeploymentSide(bool enabled, bool isHost)
{
    m_restrictPvpDeployment = enabled;
    m_localIsHost = isHost;
    update();
}

void DeployView::setPvpMapLayout(const game::ui::PvpMapLayout& layout)
{
    m_pvpLayout = layout;
    update();
}

void DeployView::setUnitVisualScale(double scale)
{
    m_unitVisualScale = std::max(0.5, scale);
    update();
}

QRectF cardSourceRect(game::core::CardKind kind)
{
    switch (kind) {
    case game::core::CardKind::Produce: return {272, 214, 151, 199};
    case game::core::CardKind::Sniper: return {440, 214, 162, 199};
    case game::core::CardKind::Specialist: return {619, 214, 160, 199};
    case game::core::CardKind::Heal: return {798, 214, 161, 199};
    case game::core::CardKind::Attack: return {973, 214, 158, 199};
    case game::core::CardKind::Aoe: return {272, 428, 153, 198};
    case game::core::CardKind::HeavyMedic: return {441, 428, 161, 198};
    case game::core::CardKind::Arsenal: return {619, 428, 160, 198};
    case game::core::CardKind::Attack2: return {795, 423, 160, 201};
    case game::core::CardKind::Heal2: return {968, 423, 164, 201};
    }
    return {440, 214, 162, 199};
}

void DeployView::setArtworkOverlayMode(bool enabled)
{
    m_artworkOverlayMode = enabled;
    setAttribute(Qt::WA_TranslucentBackground, enabled);
    setAutoFillBackground(!enabled);
    if (enabled) {
        setMinimumSize(0, 0);
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    } else {
        setMapSize(m_mapRows, m_mapCols);
    }
    update();
}

double DeployView::cellWidth() const
{
    return m_mapCols > 0 ? static_cast<double>(width()) / m_mapCols : CELL_SIZE;
}

double DeployView::cellHeight() const
{
    return m_mapRows > 0 ? static_cast<double>(height()) / m_mapRows : CELL_SIZE;
}

double DeployView::cellExtent() const
{
    return std::min(cellWidth(), cellHeight());
}

QRectF DeployView::cellRect(int row, int col) const
{
    return QRectF(col * cellWidth(), row * cellHeight(), cellWidth(), cellHeight());
}

QPointF DeployView::cellCenter(int row, int col) const
{
    return cellRect(row, col).center();
}

int DeployView::rowAtPixel(int y) const
{
    return cellHeight() > 0.0 ? static_cast<int>(std::floor(y / cellHeight())) : -1;
}

int DeployView::colAtPixel(int x) const
{
    return cellWidth() > 0.0 ? static_cast<int>(std::floor(x / cellWidth())) : -1;
}

void DeployView::updateFromSnapshot(const game::core::BattleSnapshot &snapshot)
{
    for (const auto &unit : snapshot.units) {
        bool existed = false;
        for (const auto &oldUnit : m_snapshot.units) {
            if (oldUnit.id == unit.id) {
                existed = true;
                break;
            }
        }
        if (!existed) {
            constexpr int MaxDustEffects = 20;
            if (m_dustEffects.size() >= MaxDustEffects) {
                m_dustEffects.removeFirst();
            }
            m_dustEffects.append({unit.row, unit.col, 0.64});
            AudioManager::instance().playDeploy();
        }
    }
    m_snapshot = snapshot;
    if (snapshot.map.rows > 0 && snapshot.map.cols > 0) {
        if (m_mapRows != snapshot.map.rows || m_mapCols != snapshot.map.cols) {
            setMapSize(snapshot.map.rows, snapshot.map.cols);
        }
    }
    update();
}

void DeployView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    drawTerrain(painter);
    if (m_showGrid) {
        painter.setPen(QPen(QColor(76, 58, 36, 42), 1));
        painter.setBrush(Qt::NoBrush);
        for (int row = 0; row < m_mapRows; ++row) {
            for (int col = 0; col < m_mapCols; ++col) {
                painter.drawRect(cellRect(row, col));
            }
        }
    }
    drawDeployable(painter);
    drawUnits(painter);
    drawDustEffects(painter);
    drawHoverCell(painter);

    if (m_mode == InteractionMode::RADIAL_MENU && m_selectedUnitId > 0) {
        static const QPixmap commandRing(":/images/new_art/command_ring.png");
        for (const auto &unit : m_snapshot.units) {
            if (unit.id != m_selectedUnitId) continue;
            const QRectF ringRect = commandRingRect(cellCenter(unit.row, unit.col),
                                                    cellExtent(), size());
            painter.drawPixmap(ringRect.toRect(), commandRing, commandRing.rect());
            break;
        }
        m_btnUpgrade->raise();
        m_btnMove->raise();
        m_btnRecall->raise();
    }
}

void DeployView::setShowGrid(bool show)
{
    m_showGrid = show;
    update();
}

void DeployView::drawTerrain(QPainter &painter)
{
    if (m_artworkOverlayMode) {
        return;
    }

    for (const auto &grid : m_snapshot.map.grids) {
        QRectF cell = cellRect(grid.row, grid.col);

        QColor color;
        switch (grid.terrain) {
        case game::core::TerrainType::Path:
            color = QColor(60, 60, 60);
            break;
        case game::core::TerrainType::HighGround:
            color = QColor(80, 120, 80);
            break;
        case game::core::TerrainType::FlatLand:
            color = QColor(40, 40, 40);
            break;
        case game::core::TerrainType::NoDeploy:
            color = QColor(20, 20, 20);
            break;
        case game::core::TerrainType::SpawnPoint:
            color = QColor(150, 50, 50);  // 红色出生点
            break;
        case game::core::TerrainType::CoreA:
            color = QColor(50, 100, 200);  // 蓝色A方核心
            break;
        case game::core::TerrainType::CoreB:
            color = QColor(200, 50, 50);   // 红色B方核心
            break;
        }

        painter.setPen(QPen(QColor(80, 80, 80), 1));
        painter.setBrush(color);
        painter.drawRect(cell);

        // 在出生点和核心上绘制标记
        if (grid.terrain == game::core::TerrainType::SpawnPoint) {
            painter.setPen(QPen(QColor(255, 255, 255), 2));
            painter.drawText(cell, Qt::AlignCenter, "S");
        } else if (grid.terrain == game::core::TerrainType::CoreA) {
            painter.setPen(QPen(QColor(255, 255, 255), 2));
            painter.drawText(cell, Qt::AlignCenter, "A");
        } else if (grid.terrain == game::core::TerrainType::CoreB) {
            painter.setPen(QPen(QColor(255, 255, 255), 2));
            painter.drawText(cell, Qt::AlignCenter, "B");
        }
    }
}

void DeployView::drawDeployable(QPainter &painter)
{
    if (m_mode != InteractionMode::DEPLOYING && m_mode != InteractionMode::MOVING) return;

    if (m_mode == InteractionMode::MOVING) {
        for (const auto& pos : getMovableCells(m_selectedUnitId)) {
            QRectF cell = cellRect(pos.row, pos.col);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 213, 79, 48));
            painter.drawRect(cell);
        }
        return;
    }

    for (const auto &grid : m_snapshot.map.grids) {
        if (!grid.occupied &&
            (grid.terrain == game::core::TerrainType::FlatLand ||
             grid.terrain == game::core::TerrainType::HighGround)) {
            if (m_restrictPvpDeployment &&
                !game::ui::isPvpDeploymentCellForHost(m_pvpLayout, m_localIsHost, {grid.row, grid.col})) {
                continue;
            }
            QRectF cell = cellRect(grid.row, grid.col);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 255, 0, 40));
            painter.drawRect(cell);
        }
    }
}

void DeployView::drawUnits(QPainter &painter)
{
    for (const auto &unit : m_snapshot.units) {
        QRectF unitRect = cellRect(unit.row, unit.col);
        const qreal inset = qMax<qreal>(2.0, cellExtent() * 0.09);
        QRectF innerRect = unitRect.adjusted(inset, inset, -inset, -inset);

        // 根据单位类型和阵营选择颜色
        QColor unitColor;
        QColor borderColor;
        QString label;

        // 判断是己方还是对方单位（通过 ID 范围：己方 1-999，对方 1000+）
        bool isOpponent = (unit.id >= 1000);

        if (isOpponent) {
            // 对方单位：红色系
            borderColor = QColor(255, 80, 80);
            switch (unit.type) {
            case game::core::ObjectType::CardAttack:
                unitColor = QColor(200, 60, 60);
                label = "敌";
                break;
            case game::core::ObjectType::CardProduce:
                unitColor = QColor(60, 200, 60);
                label = "敌";
                break;
            case game::core::ObjectType::CardHeal:
                unitColor = QColor(60, 60, 200);
                label = "敌";
                break;
            default:
                unitColor = QColor(150, 150, 150);
                label = "敌";
            }
        } else {
            // 己方单位：蓝色/绿色系
            borderColor = QColor(80, 200, 255);
            switch (unit.type) {
            case game::core::ObjectType::CardAttack:
                unitColor = QColor(100, 150, 255);
                label = "攻";
                break;
            case game::core::ObjectType::CardProduce:
                unitColor = QColor(100, 255, 100);
                label = "产";
                break;
            case game::core::ObjectType::CardHeal:
                unitColor = QColor(100, 200, 255);
                label = "医";
                break;
            default:
                unitColor = QColor(200, 200, 200);
                label = "?";
            }
        }

        // 绘制单位背景
        const QPixmap* sprite = unitArtwork(unit);
        if (sprite && !sprite->isNull()) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(30, 20, 12, 78));
            const qreal phase = (m_animFrame + unit.id * 11) * 0.16;
            const qreal bob = qSin(phase) * cellExtent() * 0.035 * m_unitVisualScale;
            const qreal scale = m_unitVisualScale * (1.0 + qSin(phase + 0.8) * 0.018);
            const qreal extent = cellExtent();
            const QRectF shadowRect(unitRect.center().x() - extent * 0.42 * scale,
                                    unitRect.bottom() - extent * 0.18,
                                    extent * 0.84 * scale,
                                    extent * 0.24);
            painter.drawEllipse(shadowRect);
            const qreal spriteW = extent * 1.34 * scale;
            const qreal spriteH = extent * 1.40 * scale;
            const QRectF spriteRect(unitRect.center().x() - spriteW * 0.5,
                                    unitRect.bottom() - extent * 1.38 + bob,
                                    spriteW,
                                    spriteH);
            painter.save();
            if (isOpponent) painter.setOpacity(0.76);
            drawCachedArtwork(painter, spriteRect, *sprite);
            painter.restore();
        } else {
            painter.setPen(QPen(borderColor, 2));
            painter.setBrush(unitColor);
            painter.drawRoundedRect(innerRect, 6, 6);
        }

        if (unit.id == m_selectedUnitId) {
            painter.setPen(QPen(QColor(255, 213, 79), 3));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(innerRect.adjusted(-2, -2, 2, 2), 8, 8);
        }

        // 绘制单位标签
        if (!sprite) {
            painter.setPen(QColor(255, 255, 255));
            QFont font("Microsoft YaHei", 10, QFont::Bold);
            painter.setFont(font);
            painter.drawText(innerRect, Qt::AlignCenter, label);
        }

        const int barWidth = std::max(1, static_cast<int>(innerRect.width()) - 2);
        const int barHeight = qMax(3, static_cast<int>(cellExtent() * 0.08));
        const int barX = static_cast<int>(innerRect.x()) + 1;
        const int barY = static_cast<int>(innerRect.bottom()) - barHeight - 2;
        const double hpRatio = unit.maxHp > 0
                                   ? std::clamp(static_cast<double>(unit.hp) / unit.maxHp,
                                                0.0, 1.0)
                                   : 0.0;

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(30, 30, 30, 180));
        painter.drawRoundedRect(barX, barY, barWidth, barHeight, barHeight / 2.0,
                                barHeight / 2.0);
        const int hpWidth = static_cast<int>(barWidth * hpRatio);
        if (hpWidth > 0) {
            QLinearGradient hpGrad(barX, barY, barX + hpWidth, barY);
            if (isOpponent) {
                hpGrad.setColorAt(0, QColor(190, 120, 255));
                hpGrad.setColorAt(1, QColor(118, 75, 230));
            } else {
                hpGrad.setColorAt(0, QColor(80, 220, 90));
                hpGrad.setColorAt(1, QColor(35, 175, 70));
            }
            painter.setBrush(hpGrad);
            painter.drawRoundedRect(barX, barY, hpWidth, barHeight, barHeight / 2.0,
                                    barHeight / 2.0);
        }
    }
}

void DeployView::drawHoverCell(QPainter &painter)
{
    if (m_hoverRow < 0 || m_hoverCol < 0) return;
    if (m_hoverRow >= m_mapRows || m_hoverCol >= m_mapCols) return;

    QRectF hoverRect = cellRect(m_hoverRow, m_hoverCol);
    painter.setPen(QPen(QColor(0, 212, 255, 70), 2));
    painter.setBrush(QColor(0, 212, 255, 18));
    painter.drawRect(hoverRect);
}

void DeployView::mouseMoveEvent(QMouseEvent *event)
{
    int col = colAtPixel(event->pos().x());
    int row = rowAtPixel(event->pos().y());

    if (row != m_hoverRow || col != m_hoverCol) {
        m_hoverRow = row;
        m_hoverCol = col;
        update();
    }
}

void DeployView::mousePressEvent(QMouseEvent *event)
{
    int col = colAtPixel(event->pos().x());
    int row = rowAtPixel(event->pos().y());

    if (row < 0 || row >= m_mapRows || col < 0 || col >= m_mapCols) return;

    if (m_mode == InteractionMode::NONE) {
        int unitId = findOwnUnitAt(row, col);
        if (unitId > 0) {
            m_selectedUnitId = unitId;
            m_mode = InteractionMode::RADIAL_MENU;
            const QPointF center = cellCenter(row, col);
            showRadialMenu(unitId, qRound(center.x()), qRound(center.y()));
            update();
        }
        return;
    }

    if (m_mode == InteractionMode::RADIAL_MENU) {
        if (!m_btnUpgrade->geometry().contains(event->pos()) &&
            !m_btnMove->geometry().contains(event->pos()) &&
            !m_btnRecall->geometry().contains(event->pos())) {
            hideRadialMenu();
            m_mode = InteractionMode::NONE;
            m_selectedUnitId = -1;
            update();
        }
        return;
    }

    if (m_mode == InteractionMode::MOVING) {
        bool canMove = false;
        for (const auto& pos : getMovableCells(m_selectedUnitId)) {
            if (pos.row == row && pos.col == col) {
                canMove = true;
                break;
            }
        }
        if (canMove && m_selectedUnitId > 0) {
            emit signalMoveUnit(m_selectedUnitId, game::core::MapPosition(row, col));
            m_mode = InteractionMode::NONE;
            m_selectedUnitId = -1;
            update();
        } else if (event->button() == Qt::RightButton) {
            m_mode = InteractionMode::NONE;
            m_selectedUnitId = -1;
            update();
        }
        return;
    }

    // 检查是否可部署
    bool canDeploy = false;
    for (const auto &grid : m_snapshot.map.grids) {
        if (grid.row == row && grid.col == col &&
            !grid.occupied &&
            (grid.terrain == game::core::TerrainType::FlatLand ||
             grid.terrain == game::core::TerrainType::HighGround)) {
            if (m_restrictPvpDeployment &&
                !game::ui::isPvpDeploymentCellForHost(m_pvpLayout, m_localIsHost, {row, col})) {
                break;
            }
            canDeploy = true;
            break;
        }
    }

    if (canDeploy && m_mode == InteractionMode::DEPLOYING) {
        emit signalDeployCard(m_selectedCardKind, game::core::MapPosition(row, col));
        m_mode = InteractionMode::NONE;
        update();
    }
}

QVector<game::core::MapPosition> DeployView::getDeployableCells() const
{
    QVector<game::core::MapPosition> result;
    for (const auto &grid : m_snapshot.map.grids) {
        if (!grid.occupied &&
            (grid.terrain == game::core::TerrainType::FlatLand ||
             grid.terrain == game::core::TerrainType::HighGround)) {
            if (m_restrictPvpDeployment &&
                !game::ui::isPvpDeploymentCellForHost(m_pvpLayout, m_localIsHost, {grid.row, grid.col})) {
                continue;
            }
            result.append(game::core::MapPosition(grid.row, grid.col));
        }
    }
    return result;
}

QVector<game::core::MapPosition> DeployView::getMovableCells(int unitId) const
{
    QVector<game::core::MapPosition> result;
    game::core::MapPosition unitPos;
    int moveLimit = 0;
    bool found = false;
    for (const auto& unit : m_snapshot.units) {
        if (unit.id == unitId) {
            unitPos = game::core::MapPosition(unit.row, unit.col);
            moveLimit = unit.moveLimit;
            found = true;
            break;
        }
    }
    if (!found) return result;

    for (const auto& grid : m_snapshot.map.grids) {
        game::core::MapPosition pos(grid.row, grid.col);
        int dist = unitPos.manhattanDistanceTo(pos);
        if (dist > 0 && dist <= moveLimit && !grid.occupied &&
            (grid.terrain == game::core::TerrainType::FlatLand ||
             grid.terrain == game::core::TerrainType::HighGround)) {
            result.append(pos);
        }
    }
    return result;
}

int DeployView::findOwnUnitAt(int row, int col) const
{
    for (const auto& unit : m_snapshot.units) {
        if (unit.id > 0 && unit.id < 1000 && unit.row == row && unit.col == col) {
            return unit.id;
        }
    }
    return -1;
}

void DeployView::showRadialMenu(int unitId, int pixelX, int pixelY)
{
    int level = 1;
    for (const auto& unit : m_snapshot.units) {
        if (unit.id == unitId) {
            level = unit.level;
            break;
        }
    }

    const QRectF ring = commandRingRect(QPointF(pixelX, pixelY), cellExtent(), size());
    m_btnUpgrade->setGeometry(commandButtonRect(ring, QRectF(0.02, 0.02, 0.52, 0.31)));
    m_btnMove->setGeometry(commandButtonRect(ring, QRectF(0.42, 0.20, 0.50, 0.38)));
    m_btnRecall->setGeometry(commandButtonRect(ring, QRectF(0.65, 0.57, 0.34, 0.42)));

    m_btnUpgrade->setEnabled(level < game::core::constants::MaxCardLevel);
    m_btnUpgrade->setText(QString());
    m_btnMove->setText(QString());
    m_btnRecall->setText(QString());

    m_btnUpgrade->show();
    m_btnMove->show();
    m_btnRecall->show();
    m_btnUpgrade->raise();
    m_btnMove->raise();
    m_btnRecall->raise();
}

void DeployView::hideRadialMenu()
{
    if (m_btnUpgrade) m_btnUpgrade->hide();
    if (m_btnMove) m_btnMove->hide();
    if (m_btnRecall) m_btnRecall->hide();
}

// ============================================================================
// DeployPage 实现
// ============================================================================

DeployPage::DeployPage(QWidget *parent)
    : QWidget(parent)
    , m_deployView(nullptr)
    , m_titleLabel(nullptr)
    , m_phaseLabel(nullptr)
    , m_deployCountLabel(nullptr)
    , m_localCoreLabel(nullptr)
    , m_enemyCoreLabel(nullptr)
    , m_btnBack(nullptr)
    , m_btnStartBattle(nullptr)
    , m_isPvp(false)
    , m_isHost(false)
    , m_battleManager(nullptr)
    , m_deployedCount(0)
    , m_selectedUnitId(-1)
    , m_localReady(false)
    , m_opponentReady(false)
    , m_pendingHostStart(false)
    , m_battleStartEmitted(false)
    , m_deploymentCancelled(false)
    , m_deploymentRound(1)
    , m_opponentLabel(nullptr)
    , m_pvpArtwork(":/images/artwork/battle_pvp.png")
    , m_pvpOfficeMapArtwork(":/images/artwork/battle_pvp_office_map.png")
    , m_deckArtwork(":/images/artwork/deck_atlas.png")
{
    initUI();
    connectSignals();
}

void DeployView::drawDustEffects(QPainter &painter)
{
    for (const DustEffect &effect : m_dustEffects) {
        const qreal progress = 1.0 - effect.life / 0.64;
        QPointF center = cellCenter(effect.row, effect.col);
        center.setY(cellRect(effect.row, effect.col).top()
                    + cellRect(effect.row, effect.col).height() * 0.76);
        const qreal extent = cellExtent();
        for (int i = 0; i < 9; ++i) {
            const qreal angle = i * 0.70;
            const qreal distance = extent * progress * (0.22 + (i % 3) * 0.05);
            const QPointF pos = center + QPointF(qCos(angle) * distance,
                                                 qSin(angle) * distance * 0.42);
            const qreal radius = 5.2 - progress * 2.7 + (i % 2);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(177, 139, 82, qRound((1.0 - progress) * 170)));
            painter.drawEllipse(pos, radius, radius * 0.62);
        }
    }
}

void DeployPage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(26, 55, 49));

    const QRect canvas = artworkRect();
    auto mapped = [&](const QRectF& designRect) {
        const qreal sx = canvas.width() / 1672.0;
        const qreal sy = canvas.height() / 941.0;
        return QRectF(canvas.x() + designRect.x() * sx,
                      canvas.y() + designRect.y() * sy,
                      designRect.width() * sx,
                      designRect.height() * sy);
    };
    const auto pvpLayout = game::ui::makePvpMapLayout(m_netCtx.pvpMapId.toStdString());

    if (!m_pvpArtwork.isNull()) {
        if (m_netCtx.pvpMapId == "pvp_office_panic" && !m_pvpOfficeMapArtwork.isNull()) {
            painter.drawPixmap(mapped(QRectF(0, 96, 1672, 604)),
                               m_pvpOfficeMapArtwork, pvpLayout.backgroundSourceRect);
        } else {
            painter.drawPixmap(mapped(QRectF(0, 96, 1672, 604)),
                               m_pvpArtwork, pvpLayout.backgroundSourceRect);
        }
        static QPixmap pvpHud(":/images/artwork/battle_pvp_hud_clean.png");
        const QPixmap& hudArtwork = pvpHud.isNull() ? m_pvpArtwork : pvpHud;
        painter.drawPixmap(mapped(QRectF(0, 0, 1672, 126)),
                           hudArtwork, QRectF(0, 0, 1672, 126));
        int localCoreHealth = game::core::constants::InitialBaseHealth;
        int opponentCoreHealth = game::core::constants::InitialBaseHealth;
        if (m_battleManager) {
            const auto snapshot = m_battleManager->snapshot();
            localCoreHealth = snapshot.baseHealth;
            opponentCoreHealth = snapshot.opponentBaseHealth;
        }
        drawHealthBar(painter, mapped(QRectF(615, 72, 194, 12)),
                      localCoreHealth,
                      game::core::constants::InitialBaseHealth,
                      QColor(62, 157, 203));
        drawHealthBar(painter, mapped(QRectF(918, 72, 194, 12)),
                      opponentCoreHealth,
                      game::core::constants::InitialBaseHealth,
                      QColor(205, 83, 61));
        painter.drawPixmap(mapped(QRectF(0, 690, 1672, 251)),
                           m_pvpArtwork, QRectF(0, 690, 1672, 251));

        const QRectF cardDestinations[] = {
            {355, 704, 180, 216}, {550, 704, 180, 216},
            {745, 704, 180, 216}, {940, 704, 180, 216},
            {1135, 704, 180, 216}
        };
        for (int i = 0; i < 5; ++i) {
            if (i < m_deck.size()) {
                painter.drawPixmap(mapped(cardDestinations[i]),
                                   m_deckArtwork, cardSourceRect(m_deck[i]));
            }
        }
    }
}

void DeployPage::setNetworkContext(const NetworkContext& ctx)
{
    m_netCtx = ctx;
    m_isPvp = ctx.isPvp;
    m_isHost = ctx.isHost;
    if (m_deployView) {
        m_deployView->setPvpDeploymentSide(m_isPvp, m_isHost);
        m_deployView->setPvpMapLayout(game::ui::makePvpMapLayout(m_netCtx.pvpMapId.toStdString()));
    }
    layoutArtworkUi();
    update();
}

void DeployPage::setDeck(const QVector<game::core::CardKind>& deck)
{
    m_deck = deck.mid(0, 5);
    refreshCardDisplay();
    update();
}

void DeployPage::initUI()
{
    setAutoFillBackground(false);
    m_deployView = new DeployView(this);
    m_deployView->setArtworkOverlayMode(true);

    const QString labelStyle =
        "QLabel { color: #3B2819; background: #F2DCA9;"
        " border-radius: 4px;"
        " font-size: 18px; font-weight: 700; padding: 2px 6px; }";
    m_titleLabel = new QLabel("0", this);
    m_phaseLabel = new QLabel("Resource", this);
    m_deployCountLabel = new QLabel("0", this);
    m_localCoreLabel = new QLabel("10/10", this);
    m_enemyCoreLabel = new QLabel("10/10", this);
    m_opponentLabel = new QLabel("...", this);
    for (QLabel *label : {m_titleLabel, m_phaseLabel, m_deployCountLabel, m_localCoreLabel,
                          m_enemyCoreLabel, m_opponentLabel}) {
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet(labelStyle);
    }

    const QString commandStyle =
        "QPushButton { background: rgba(246,226,176,225); color: #3B2819;"
        " border: 2px solid rgba(89,61,36,195); border-radius: 8px;"
        ""
        " font-size: 18px; font-weight: 700; }"
        "QPushButton:hover { background: rgba(255,239,191,245); border-color: #F8D77A; }"
        "QPushButton:pressed { background: rgba(197,159,94,235); }"
        "QPushButton:disabled { color: rgba(80,61,42,145); background: rgba(222,203,161,185); }";

    m_btnBack = new QPushButton("Back", this);
    m_btnBack->setCursor(Qt::PointingHandCursor);
    m_btnBack->setStyleSheet(commandStyle);

    for (int i = 0; i < 5; ++i) {
        auto *cardBtn = new QPushButton(this);
        cardBtn->setText(QString());
        cardBtn->setCursor(Qt::PointingHandCursor);
        cardBtn->setEnabled(false);
        cardBtn->setStyleSheet(
            "QPushButton { background: transparent; border: 3px solid transparent; border-radius: 6px; }"
            "QPushButton:hover { background: rgba(255,245,194,42); border-color: rgba(255,236,139,210); }"
            "QPushButton:pressed { background: rgba(111,78,39,70); }"
            "QPushButton:disabled { background: rgba(44,35,28,28); border-color: transparent; }");
        m_cardButtons.append(cardBtn);

        auto *nameLabel = new QLabel(this);
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        nameLabel->setStyleSheet(
            "QLabel { color: #332216; background: #F1DCA9;"
            " border: 1px solid rgba(104,72,39,120); border-radius: 3px;"
            " font-size: 12px; font-weight: 700; padding: 1px 3px; }");
        m_cardNameLabels.append(nameLabel);

        auto *costLabel = new QLabel(this);
        costLabel->setAlignment(Qt::AlignCenter);
        costLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        costLabel->setStyleSheet(
            "QLabel { color: #342113; background: #F1DCA9;"
            " border: 2px solid #74502C; border-radius: 10px;"
            " font-size: 15px; font-weight: 800; padding: 1px 5px; }");
        m_cardCostLabels.append(costLabel);
    }

    m_btnStartBattle = new QPushButton("Ready", this);
    m_btnStartBattle->setCursor(Qt::PointingHandCursor);
    m_btnStartBattle->setStyleSheet(commandStyle);

    setStyleSheet("DeployPage { background-color: #203B35; }");

    // 设置卡牌按钮连接（只一次）
    setupCardButtonConnections();
    refreshCardDisplay();
    layoutArtworkUi();
}

QRect DeployPage::artworkRect() const
{
    constexpr int DesignWidth = 1672;
    constexpr int DesignHeight = 941;
    QSize fitted(DesignWidth, DesignHeight);
    fitted.scale(size(), Qt::KeepAspectRatio);
    return QRect((width() - fitted.width()) / 2,
                 (height() - fitted.height()) / 2,
                 fitted.width(), fitted.height());
}

void DeployPage::layoutArtworkUi()
{
    if (!m_deployView) return;

    const QRect canvas = artworkRect();
    const auto pvpLayout = game::ui::makePvpMapLayout(m_netCtx.pvpMapId.toStdString());
    const qreal sx = canvas.width() / 1672.0;
    const qreal sy = canvas.height() / 941.0;
    auto mapped = [&](const QRectF &designRect) {
        return QRect(qRound(canvas.x() + designRect.x() * sx),
                     qRound(canvas.y() + designRect.y() * sy),
                     qMax(1, qRound(designRect.width() * sx)),
                     qMax(1, qRound(designRect.height() * sy)));
    };

    const int fontPx = qMax(11, qRound(21 * std::min(sx, sy)));
    const QString labelStyle = QString(
        "QLabel { color: #3B2819; background: transparent;"
        " border: none;"
        " font-size: %1px; font-weight: 900; padding: 0px; }")
        .arg(fontPx);
    for (QLabel *label : {m_titleLabel, m_deployCountLabel, m_localCoreLabel,
                          m_phaseLabel, m_enemyCoreLabel, m_opponentLabel}) {
        label->setStyleSheet(labelStyle);
    }

    const qreal uiScale = std::min(sx, sy);
    const int namePx = qMax(9, qRound(13 * uiScale));
    const int costPx = qMax(10, qRound(15 * uiScale));
    const QString cardNameStyle = QString(
        "QLabel { color: #332216; background: rgba(241,220,169,235);"
        " border: %1px solid rgba(104,72,39,120); border-radius: %2px;"
        " font-size: %3px; font-weight: 700; padding: 0px 2px; }")
        .arg(qMax(1, qRound(1 * uiScale)))
        .arg(qMax(2, qRound(3 * uiScale)))
        .arg(namePx);
    const QString cardCostStyle = QString(
        "QLabel { color: #342113; background: rgba(246,226,176,245);"
        " border: %1px solid #74502C; border-radius: %2px;"
        " font-size: %3px; font-weight: 850; padding: 0px 2px; }")
        .arg(qMax(1, qRound(2 * uiScale)))
        .arg(qMax(5, qRound(10 * uiScale)))
        .arg(costPx);
    for (QLabel *label : m_cardNameLabels) {
        label->setStyleSheet(cardNameStyle);
    }
    for (QLabel *label : m_cardCostLabels) {
        label->setStyleSheet(cardCostStyle);
    }

    const QRectF deployRect = m_isPvp ? pvpLayout.deployViewRect
                                      : QRectF(174, 126, 1324, 552);
    m_deployView->setGeometry(mapped(deployRect));
    m_titleLabel->setGeometry(mapped(QRectF(210, 36, 52, 50)));
    m_phaseLabel->setGeometry(mapped(QRectF(416, 36, 104, 50)));
    m_localCoreLabel->setGeometry(mapped(QRectF(736, 30, 104, 44)));
    m_enemyCoreLabel->setGeometry(mapped(QRectF(1048, 30, 72, 44)));
    m_deployCountLabel->setGeometry(mapped(QRectF(1288, 36, 54, 50)));
    m_opponentLabel->setGeometry(mapped(QRectF(1462, 36, 68, 50)));
    m_btnBack->setGeometry(mapped(QRectF(90, 775, 190, 64)));
    m_btnStartBattle->setGeometry(mapped(QRectF(1380, 775, 205, 64)));

    const QRectF cards[] = {
        {355, 704, 180, 216}, {550, 704, 180, 216},
        {745, 704, 180, 216}, {940, 704, 180, 216},
        {1135, 704, 180, 216}
    };
    for (int i = 0; i < m_cardButtons.size(); ++i) {
        const QRect cardRect = mapped(cards[i]);
        m_cardButtons[i]->setGeometry(cardRect);
        m_cardNameLabels[i]->setGeometry(mapped(QRectF(cards[i].x() + 15,
                                                       cards[i].y() + 10,
                                                       cards[i].width() - 30, 30)));
        m_cardCostLabels[i]->setGeometry(mapped(QRectF(cards[i].x() + 28,
                                                       cards[i].y() + 168,
                                                       cards[i].width() - 56, 34)));
    }

    m_deployView->lower();
    const QVector<QWidget*> foregroundWidgets = {
        m_titleLabel, m_phaseLabel, m_deployCountLabel, m_localCoreLabel, m_enemyCoreLabel, m_opponentLabel,
        m_btnBack, m_btnStartBattle
    };
    for (QWidget *widget : foregroundWidgets) {
        widget->raise();
    }
    for (auto *button : m_cardButtons) button->raise();
    for (auto *label : m_cardNameLabels) label->raise();
    for (auto *label : m_cardCostLabels) label->raise();
}

void DeployPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutArtworkUi();
    update();
}

void DeployPage::refreshCardDisplay()
{
    for (int i = 0; i < m_cardButtons.size(); ++i) {
        const bool hasCard = i < m_deck.size();
        m_cardButtons[i]->setVisible(hasCard);
        m_cardButtons[i]->setEnabled(hasCard && !m_localReady);
        m_cardNameLabels[i]->setVisible(hasCard);
        m_cardCostLabels[i]->setVisible(hasCard);
        if (!hasCard) continue;

        const auto kind = m_deck[i];
        const int cost = game::core::CardSystem::deployCost(kind);
        const QString name = displayCardName(kind);
        m_cardButtons[i]->setToolTip(QString("%1 - Cost %2").arg(name).arg(cost));
        m_cardNameLabels[i]->setText(name);
        m_cardCostLabels[i]->setText(QString("Juice %1").arg(cost));
    }
}

void DeployPage::connectSignals()
{
    // 返回按钮
    connect(m_btnBack, &QPushButton::clicked, this, &DeployPage::handleBackClicked);

    // 部署信号
    connect(m_deployView, &DeployView::signalDeployCard,
            this, [this](game::core::CardKind kind, game::core::MapPosition pos) {
        if (m_battleManager) {
            const auto layout = game::ui::makePvpMapLayout(m_netCtx.pvpMapId.toStdString());
            if (m_isPvp && !game::ui::isPvpDeploymentCellForHost(layout, m_isHost, pos)) {
                return;
            }
            auto result = m_battleManager->deployCard(kind, pos);
            if (result) {
                m_deployedCount++;

                if (m_isPvp) {
                    sendDeployToNetwork(kind, pos, result->id());
                }

                game::core::BattleSnapshot snap = m_battleManager->snapshot();
                m_deployView->updateFromSnapshot(snap);
                updateDeployCount();
            }
        }
    });

    connect(m_deployView, &DeployView::signalUpgradeUnit,
            this, [this](int unitId) {
        if (!m_battleManager || unitId < 0) return;
        if (m_battleManager->upgradeCard(unitId)) {
            if (m_isPvp) sendUpgradeToNetwork(unitId);
            refreshSnapshot();
        }
    });

    connect(m_deployView, &DeployView::signalMoveUnit,
            this, [this](int unitId, game::core::MapPosition pos) {
        if (!m_battleManager) return;
        if (m_battleManager->moveCard(unitId, pos)) {
            if (m_isPvp) sendMoveToNetwork(unitId, pos);
            m_selectedUnitId = -1;
            refreshSnapshot();
        }
    });

    connect(m_deployView, &DeployView::signalRecallUnit,
            this, [this](int unitId) {
        if (!m_battleManager || unitId < 0) return;
        if (m_battleManager->recallCard(unitId)) {
            if (m_isPvp) sendRecallToNetwork(unitId);
            m_selectedUnitId = -1;
            m_deployView->m_selectedUnitId = -1;
            refreshSnapshot();
        }
    });

    // 开战按钮
    connect(m_btnStartBattle, &QPushButton::clicked, this, [this]() {
        if (m_battleStartEmitted) {
            qDebug() << "[DeployPage] ignoring Ready click after battle start emitted";
            return;
        }
        m_localReady = true;
        m_btnStartBattle->setEnabled(false);
        m_btnStartBattle->setText("Waiting...");

        if (m_isPvp) {
            qInfo() << "[DeployPage] local deployment ready"
                    << "host=" << m_isHost
                    << "opponentReady=" << m_opponentReady;
            sendDeploymentEnd();
            tryStartPvpBattle("local ready clicked");
        } else {
            emit signalBattleStart();
        }
    });
}

void DeployPage::initDeployment()
{
    // 获取 BattleManager
    MainWindow *mainWin = qobject_cast<MainWindow*>(window());
    if (mainWin) {
        m_battleManager = mainWin->battleManager();
    }
    if (!m_battleManager) return;

    // 清理 BattleManager 状态
    m_battleManager->clearBattle();
    m_pendingOpponentDeploys.clear();
    m_pendingOpponentOps.clear();
    m_deployedCount = 0;
    m_selectedUnitId = -1;
    resetDeploymentSyncState();
    m_deploymentRound = qMax(1, m_battleManager->currentWave() + 1);
    m_pendingOpponentDeploys.clear();
    m_pendingOpponentOps.clear();

    // 设置地图
    setupMap();

    refreshCardDisplay();

    // 连接网络信号（只连接一次，使用 UniqueConnection）
    if (m_isPvp) {
        if (m_isHost && m_netCtx.server) {
            connect(m_netCtx.server, &game::network::GameServer::packetReceived,
                    this, &DeployPage::onNetworkPacket, Qt::UniqueConnection);
            connect(m_netCtx.server, &game::network::GameServer::clientDisconnected,
                    this, &DeployPage::handleNetworkDisconnected, Qt::UniqueConnection);
        } else if (!m_isHost && m_netCtx.client) {
            connect(m_netCtx.client, &game::network::GameClient::packetReceived,
                    this, &DeployPage::onNetworkPacket, Qt::UniqueConnection);
            connect(m_netCtx.client, &game::network::GameClient::disconnected,
                    this, &DeployPage::handleNetworkDisconnected, Qt::UniqueConnection);
        }
    }

    // 更新显示
    updateDeployCount();
    m_btnStartBattle->setEnabled(true);
    m_btnStartBattle->setText("Ready");
    m_opponentLabel->setText("...");

    game::core::BattleSnapshot snap = m_battleManager->snapshot();
    m_deployView->updateFromSnapshot(snap);
}

// ========== setupCardButtonConnections() —— 设置卡牌按钮连接（只调用一次） ==========
void DeployPage::setupCardButtonConnections()
{
    static bool connected = false;
    if (connected) return;
    connected = true;

    for (int i = 0; i < m_cardButtons.size(); ++i) {
        connect(m_cardButtons[i], &QPushButton::clicked, this, [this, i]() {
            if (i >= m_deck.size() || !m_cardButtons[i]->isEnabled()) return;

            game::core::CardKind kind = m_deck[i];
            m_deployView->m_selectedCardKind = kind;
            m_deployView->m_mode = DeployView::InteractionMode::DEPLOYING;

            // 高亮选中按钮
            for (int j = 0; j < m_cardButtons.size(); ++j) {
                m_cardButtons[j]->setStyleSheet(
                    "QPushButton { background: transparent; border: 3px solid transparent; border-radius: 6px; }"
                    "QPushButton:hover { background: rgba(255,245,194,42); border-color: rgba(255,236,139,210); }"
                );
            }
            m_cardButtons[i]->setStyleSheet(
                "QPushButton { background: rgba(255,234,133,46);"
                " border: 4px solid rgba(255,226,104,235); border-radius: 7px; }"
                "QPushButton:hover { background: rgba(255,245,194,62); border-color: #FFF0A3; }"
            );
        });
    }
}

void DeployPage::setupMap()
{
    auto& map = m_battleManager->map();
    const auto layout = game::ui::makePvpMapLayout(m_netCtx.pvpMapId.toStdString());
    game::ui::applyPvpMapLayout(map, layout);

    m_battleManager->rebuildMapOccupancy();
    m_battleManager->setPaths({layout.pathToA, layout.pathToB});
    m_deployView->setMapSize(map.rows(), map.cols());
    m_deployView->setPvpMapLayout(layout);
    m_deployView->setUnitVisualScale(layout.unitVisualScale);
    m_deployView->m_spawnPos = layout.spawnA;
    m_deployView->m_corePos = m_isHost ? layout.coreA : layout.coreB;
}

void DeployPage::updateDeployCount()
{
    int resources = m_battleManager ? m_battleManager->resources().resources() : 0;
    m_titleLabel->setText(QString::number(qMax(1, m_deploymentRound)));
    m_phaseLabel->setText("Resource");
    m_deployCountLabel->setText(QString::number(resources));
    if (m_battleManager) {
        const auto snapshot = m_battleManager->snapshot();
        m_localCoreLabel->setText(QString("%1/10").arg(snapshot.baseHealth));
        m_enemyCoreLabel->setText(QString("%1/10").arg(snapshot.opponentBaseHealth));
    }
}

void DeployPage::refreshSnapshot()
{
    if (!m_battleManager) return;
    int deployed = 0;
    for (const auto& card : m_battleManager->cardSystem().cards()) {
        if (card && !card->isDead()) deployed++;
    }
    m_deployedCount = deployed;
    updateDeployCount();
    m_deployView->updateFromSnapshot(m_battleManager->snapshot());
}

// ========== reEnter() —— 重新进入部署阶段 ==========
void DeployPage::reEnter()
{
    // 不用 clearBattle，保留现有单位
    resetDeploymentSyncState();
    m_deploymentRound = qMax(1, m_battleManager ? m_battleManager->currentWave() + 1 : m_deploymentRound + 1);
    m_btnStartBattle->setEnabled(true);
    m_btnStartBattle->setText("Ready");
    m_opponentLabel->setText("...");
    m_selectedUnitId = -1;
    m_deployView->m_selectedUnitId = -1;
    m_deployView->hideRadialMenu();

    // 启用卡牌按钮
    for (int i = 0; i < m_cardButtons.size(); ++i) {
        m_cardButtons[i]->setEnabled(i < m_deck.size());
    }

    // 更新计数和资源
    int deployed = 0;
    if (m_battleManager) {
        for (const auto& card : m_battleManager->cardSystem().cards()) {
            if (card && !card->isDead()) deployed++;
        }
    }
    m_deployedCount = deployed;
    updateDeployCount();

    // 渲染
    game::core::BattleSnapshot snap = m_battleManager->snapshot();
    m_deployView->updateFromSnapshot(snap);
}

void DeployPage::resumeDeployment()
{
    reEnter();
}

void DeployPage::setShowGrid(bool show)
{
    if (m_deployView) m_deployView->setShowGrid(show);
}

void DeployPage::resetDeploymentSyncState()
{
    m_localReady = false;
    m_opponentReady = false;
    m_pendingHostStart = false;
    m_battleStartEmitted = false;
    m_deploymentCancelled = false;
}

void DeployPage::handleBackClicked()
{
    if (!m_isPvp) {
        emit signalBack();
        return;
    }

    const auto reply = QMessageBox::question(
        this,
        tr("Leave deployment?"),
        tr("Leave this PVP deployment round? Both players will return to card selection."),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (reply != QMessageBox::Yes) {
        return;
    }

    sendDeploymentCancel();
    handleDeploymentCancel();
}

void DeployPage::sendDeploymentCancel()
{
    const QByteArray body = game::network::BattleStateCodec::encodeDeploymentRound(m_deploymentRound);
    if (m_isHost && m_netCtx.server) {
        m_netCtx.server->sendPacket(game::network::MsgType::DEPLOYMENT_CANCEL, body);
    } else if (!m_isHost && m_netCtx.client) {
        m_netCtx.client->sendPacket(game::network::MsgType::DEPLOYMENT_CANCEL, body);
    }
}

void DeployPage::handleDeploymentCancel()
{
    if (m_deploymentCancelled) return;
    m_deploymentCancelled = true;
    m_pendingOpponentDeploys.clear();
    m_pendingOpponentOps.clear();
    m_localReady = false;
    m_opponentReady = false;
    m_pendingHostStart = false;
    m_battleStartEmitted = false;
    if (m_btnStartBattle) {
        m_btnStartBattle->setEnabled(false);
        m_btnStartBattle->setText("Cancelled");
    }
    emit signalBack();
}

void DeployPage::sendDeployToNetwork(game::core::CardKind kind, game::core::MapPosition pos,
                                     int unitId)
{
    QByteArray body = game::network::BattleStateCodec::encodeDeployAction(
        kind, pos, unitId, m_deploymentRound);

    if (m_isHost && m_netCtx.server) {
        m_netCtx.server->sendPacket(game::network::MsgType::DEPLOY, body);
    } else if (!m_isHost && m_netCtx.client) {
        m_netCtx.client->sendPacket(game::network::MsgType::DEPLOY, body);
    }
}

void DeployPage::sendUpgradeToNetwork(int unitId)
{
    QByteArray body = game::network::BattleStateCodec::encodeUpgradeAction(
        unitId, 0, m_deploymentRound);

    if (m_isHost && m_netCtx.server) {
        m_netCtx.server->sendPacket(game::network::MsgType::UPGRADE_UNIT, body);
    } else if (!m_isHost && m_netCtx.client) {
        m_netCtx.client->sendPacket(game::network::MsgType::UPGRADE_UNIT, body);
    }
}

void DeployPage::sendMoveToNetwork(int unitId, game::core::MapPosition pos)
{
    QByteArray body = game::network::BattleStateCodec::encodeMoveAction(
        unitId, pos, m_deploymentRound);

    if (m_isHost && m_netCtx.server) {
        m_netCtx.server->sendPacket(game::network::MsgType::MOVE_UNIT, body);
    } else if (!m_isHost && m_netCtx.client) {
        m_netCtx.client->sendPacket(game::network::MsgType::MOVE_UNIT, body);
    }
}

void DeployPage::sendRecallToNetwork(int unitId)
{
    QByteArray body = game::network::BattleStateCodec::encodeRecallAction(
        unitId, m_deploymentRound);

    if (m_isHost && m_netCtx.server) {
        m_netCtx.server->sendPacket(game::network::MsgType::RECALL_UNIT, body);
    } else if (!m_isHost && m_netCtx.client) {
        m_netCtx.client->sendPacket(game::network::MsgType::RECALL_UNIT, body);
    }
}

void DeployPage::handleNetworkDisconnected()
{
    m_btnStartBattle->setEnabled(false);
    m_btnStartBattle->setText("Disconnected");
    m_localReady = false;
    m_opponentReady = false;
    m_pendingHostStart = false;
    if (m_isPvp && !m_deploymentCancelled) {
        handleDeploymentCancel();
    }
}

void DeployPage::applyPendingOpponentDeploys()
{
    if (!m_battleManager || m_pendingOpponentDeploys.isEmpty()) return;

    for (const auto& pending : m_pendingOpponentDeploys) {
        const auto result = m_battleManager->revealOpponentDeploy(
            pending.kind, pending.position, pending.unitId);
        if (result.outcome == game::core::OpponentDeployRevealOutcome::Failed) {
            qDebug() << "[DeployPage] failed to reveal opponent DEPLOY"
                     << "unit=" << pending.unitId
                     << "at" << pending.position.row << pending.position.col;
        } else if (result.outcome == game::core::OpponentDeployRevealOutcome::LocalWon
                   || result.outcome == game::core::OpponentDeployRevealOutcome::OpponentWon
                   || result.outcome == game::core::OpponentDeployRevealOutcome::Draw) {
            qDebug() << "[DeployPage] resolved hidden deployment clash"
                     << "outcome=" << static_cast<int>(result.outcome)
                     << "localUnit=" << result.localUnitId
                     << "opponentUnit=" << pending.unitId
                     << "at" << pending.position.row << pending.position.col;
        }
    }
    qDebug() << "[DeployPage] revealed opponent deploys:" << m_pendingOpponentDeploys.size();
    m_pendingOpponentDeploys.clear();
}

void DeployPage::applyPendingOpponentOps()
{
    if (!m_battleManager || m_pendingOpponentOps.isEmpty()) return;

    for (const auto& op : m_pendingOpponentOps) {
        switch (op.type) {
        case game::network::MsgType::UPGRADE_UNIT:
            m_battleManager->upgradeOpponentCard(op.unitId);
            break;
        case game::network::MsgType::MOVE_UNIT:
            m_battleManager->moveOpponentCard(op.unitId, op.target);
            break;
        case game::network::MsgType::RECALL_UNIT:
            m_battleManager->recallOpponentCard(op.unitId);
            break;
        default:
            break;
        }
    }
    qDebug() << "[DeployPage] revealed opponent ops:" << m_pendingOpponentOps.size();
    m_pendingOpponentOps.clear();
}

void DeployPage::tryStartPvpBattle(const char* reason)
{
    if (!m_isPvp) {
        if (!m_battleStartEmitted) {
            m_battleStartEmitted = true;
            emit signalBattleStart();
        }
        return;
    }

    if (m_battleStartEmitted) {
        qDebug() << "[DeployPage] ignoring duplicate battle start"
                 << "reason=" << reason
                 << "host=" << m_isHost;
        return;
    }

    if (m_isHost) {
        if (!m_localReady || !m_opponentReady) {
            qDebug() << "[DeployPage][Host] deployment battle not ready"
                     << "reason=" << reason
                     << "localReady=" << m_localReady
                     << "opponentReady=" << m_opponentReady;
            return;
        }

        applyPendingOpponentDeploys();
        applyPendingOpponentOps();
        if (m_netCtx.server) {
            qInfo() << "[DeployPage][Host] broadcasting deployment DEPLOYMENT_START"
                    << "reason=" << reason;
            m_netCtx.server->sendPacket(
                game::network::MsgType::DEPLOYMENT_START,
                game::network::BattleStateCodec::encodeDeploymentRound(m_deploymentRound));
        } else {
            qDebug() << "[DeployPage][Host] cannot broadcast DEPLOYMENT_START: server missing";
        }
        m_battleStartEmitted = true;
        emit signalBattleStart();
        return;
    }

    if (!m_localReady || !m_pendingHostStart) {
        qDebug() << "[DeployPage][Client] deployment battle not ready"
                 << "reason=" << reason
                 << "localReady=" << m_localReady
                 << "pendingHostStart=" << m_pendingHostStart;
        return;
    }

    qInfo() << "[DeployPage][Client] starting battle from host DEPLOYMENT_START"
            << "reason=" << reason
            << "localReady=" << m_localReady
            << "opponentReady=" << m_opponentReady;
    applyPendingOpponentDeploys();
    applyPendingOpponentOps();
    m_battleStartEmitted = true;
    emit signalBattleStart();
}

void DeployPage::sendDeploymentEnd()
{
    if (m_isHost && m_netCtx.server) {
        m_netCtx.server->sendPacket(
            game::network::MsgType::DEPLOYMENT_END,
            game::network::BattleStateCodec::encodeDeploymentRound(m_deploymentRound));
    } else if (!m_isHost && m_netCtx.client) {
        m_netCtx.client->sendPacket(
            game::network::MsgType::DEPLOYMENT_END,
            game::network::BattleStateCodec::encodeDeploymentRound(m_deploymentRound));
    }

    m_opponentLabel->setText("OK");
    m_opponentLabel->update();
}

void DeployPage::onNetworkPacket(game::network::MsgType type, const QByteArray& body)
{
    switch (type) {
    case game::network::MsgType::DEPLOY: {
        if (m_battleStartEmitted) break;
        // 迷雾部署阶段只缓存对方本轮新部署，不立即写入 BattleManager。
        // 这样部署阶段只能看到上一轮战斗已经暴露过的单位；本轮新部署到开战时再揭示。
        game::network::BattleStateCodec::DeployAction action;
        if (game::network::BattleStateCodec::decodeDeployAction(body, action)) {
            if (action.roundId != m_deploymentRound) {
                qDebug() << "[DeployPage] ignored stale DEPLOY"
                         << "round=" << action.roundId
                         << "current=" << m_deploymentRound;
                break;
            }
            m_pendingOpponentDeploys.append({action.cardKind, action.position, action.unitId});
            qDebug() << "[DeployPage] cached hidden opponent DEPLOY:"
                     << static_cast<int>(action.cardKind)
                     << "unit=" << action.unitId
                     << "at" << action.position.row << action.position.col;
        }
        break;
    }
    case game::network::MsgType::DEPLOYMENT_END: {
        int roundId = 0;
        if (!game::network::BattleStateCodec::decodeDeploymentRound(body, roundId)
            || roundId != m_deploymentRound) {
            qDebug() << "[DeployPage] ignored stale DEPLOYMENT_END"
                     << "round=" << roundId
                     << "current=" << m_deploymentRound;
            break;
        }
        m_opponentReady = true;
        m_opponentLabel->setText("OK");
        m_opponentLabel->update();
        qInfo() << "[DeployPage] opponent deployment ready"
                << "host=" << m_isHost
                << "localReady=" << m_localReady;
        tryStartPvpBattle("opponent deployment end");
        break;
    }
    case game::network::MsgType::DEPLOYMENT_START: {
        int roundId = 0;
        if (!game::network::BattleStateCodec::decodeDeploymentRound(body, roundId)
            || roundId != m_deploymentRound) {
            qDebug() << "[DeployPage] ignored stale DEPLOYMENT_START"
                     << "round=" << roundId
                     << "current=" << m_deploymentRound;
            break;
        }
        qInfo() << "[DeployPage] received deployment DEPLOYMENT_START"
                << "host=" << m_isHost
                << "localReady=" << m_localReady;
        if (!m_isHost) {
            m_pendingHostStart = true;
            m_opponentReady = true;
            tryStartPvpBattle("received DEPLOYMENT_START");
        }
        break;
    }
    case game::network::MsgType::DEPLOYMENT_CANCEL: {
        int roundId = 0;
        if (!game::network::BattleStateCodec::decodeDeploymentRound(body, roundId)
            || roundId != m_deploymentRound) {
            qDebug() << "[DeployPage] ignored stale DEPLOYMENT_CANCEL"
                     << "round=" << roundId
                     << "current=" << m_deploymentRound;
            break;
        }
        QMessageBox::information(this,
                                 tr("PVP deployment cancelled"),
                                 tr("The other player left deployment. Returning to card selection."));
        handleDeploymentCancel();
        break;
    }
    case game::network::MsgType::UPGRADE_UNIT: {
        if (m_battleStartEmitted) break;
        game::network::BattleStateCodec::UnitAction action;
        if (game::network::BattleStateCodec::decodeUpgradeAction(body, action)) {
            if (action.roundId != m_deploymentRound) {
                qDebug() << "[DeployPage] ignored stale UPGRADE"
                         << "round=" << action.roundId
                         << "current=" << m_deploymentRound;
                break;
            }
            m_pendingOpponentOps.append({type, action.unitId, {}});
            qDebug() << "[DeployPage] cached hidden opponent UPGRADE:" << action.unitId;
        }
        break;
    }
    case game::network::MsgType::MOVE_UNIT: {
        if (m_battleStartEmitted) break;
        game::network::BattleStateCodec::UnitAction action;
        if (game::network::BattleStateCodec::decodeMoveAction(body, action)) {
            if (action.roundId != m_deploymentRound) {
                qDebug() << "[DeployPage] ignored stale MOVE"
                         << "round=" << action.roundId
                         << "current=" << m_deploymentRound;
                break;
            }
            m_pendingOpponentOps.append({type, action.unitId, action.position});
            qDebug() << "[DeployPage] cached hidden opponent MOVE:" << action.unitId
                     << action.position.row << action.position.col;
        }
        break;
    }
    case game::network::MsgType::RECALL_UNIT: {
        if (m_battleStartEmitted) break;
        game::network::BattleStateCodec::UnitAction action;
        if (game::network::BattleStateCodec::decodeRecallAction(body, action)) {
            if (action.roundId != m_deploymentRound) {
                qDebug() << "[DeployPage] ignored stale RECALL"
                         << "round=" << action.roundId
                         << "current=" << m_deploymentRound;
                break;
            }
            m_pendingOpponentOps.append({type, action.unitId, {}});
            qDebug() << "[DeployPage] cached hidden opponent RECALL:" << action.unitId;
        }
        break;
    }
    case game::network::MsgType::DISCONNECT:
        handleNetworkDisconnected();
        handleDeploymentCancel();
        break;
    default:
        break;
    }
}
