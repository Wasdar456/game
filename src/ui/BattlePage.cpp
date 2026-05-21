/**
 * @file BattlePage.cpp
 * @brief 战斗主页面实现文件
 *
 * 核心架构说明：
 *   本文件是 UI 层与 core 层的桥梁，遵循"快照驱动渲染"的设计：
 *
 *   每帧执行流程：
 *   1. QTimer 触发 onGameTick()
 *   2. 调用 BattleManager::update(deltaSeconds) 推进游戏逻辑
 *   3. 调用 BattleManager::snapshot() 获取只读快照
 *   4. 将快照传给 BattleView::updateFromSnapshot() 渲染界面
 *   5. 更新状态栏数据
 *
 *   用户操作流程：
 *   1. 用户在 BattleView 上点击 → BattleView 发出信号
 *   2. BattlePage 的槽函数接收信号
 *   3. 调用 BattleManager 对应接口（deployCard/upgradeCard/moveCard/recallCard）
 *   4. 下一帧自动刷新界面
 */

#include "ui/BattlePage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QPen>
#include <QBrush>
#include <QConicalGradient>
#include <QRadialGradient>
#include <QLinearGradient>
#include <vector>
#include <QtMath>

// ========== 引入 MainWindow 头文件以获取 BattleManager ==========
#include "ui/MainWindow.h"

// ========== 引入核心层头文件 ==========
#include "core/systems/ResourceManager.h"  // 资源管理
#include "core/base/Constants.h"           // 游戏常量

// ============================================================================
// BattleView 实现
// ============================================================================

BattleView::BattleView(QWidget *parent)
    : QWidget(parent)
    , m_mode(InteractionMode::NONE)
    , m_selectedCardKind(game::core::CardKind::Attack)
    , m_selectedUnitId(-1)
    , m_moveRange(0)
    , m_spawnPos(1, 1)
    , m_corePos(10, 16)
    , m_btnUpgrade(nullptr)
    , m_btnMove(nullptr)
    , m_btnRetreat(nullptr)
    , m_animFrame(0)
    , m_hoverRow(-1)
    , m_hoverCol(-1)
    , m_mapRows(game::core::constants::DefaultMapRows)
    , m_mapCols(game::core::constants::DefaultMapCols)
{
    setMapSize(m_mapRows, m_mapCols);

    // ----- 创建环形菜单按钮 -----
    m_btnUpgrade = new QPushButton("⬆ 升级", this);
    m_btnMove    = new QPushButton("🏃 移动", this);
    m_btnRetreat = new QPushButton("↩ 撤回", this);

    QString radialStyle =
        "QPushButton {"
        "  background-color: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 rgba(22,50,90,0.95), stop:1 rgba(12,28,50,0.95));"
        "  color: #00E5FF;"
        "  border: 2px solid rgba(0,212,255,0.65); border-radius: 18px;"
        "  font-size: 13px; font-weight: bold; padding: 8px 14px;"
        "}"
        "QPushButton:hover { background-color: rgba(0,212,255,0.25); border: 2px solid #00D4FF; }"
        "QPushButton:disabled { color: #667788; border: 2px solid rgba(0,212,255,0.25); }"
    ;
    m_btnUpgrade->setStyleSheet(radialStyle);
    m_btnMove->setStyleSheet(radialStyle);
    m_btnRetreat->setStyleSheet(radialStyle);

    m_btnUpgrade->hide();
    m_btnMove->hide();
    m_btnRetreat->hide();

    // 连接环形菜单按钮信号
    connect(m_btnUpgrade, &QPushButton::clicked, this, [this]() {
        if (m_selectedUnitId >= 0) {
            emit signalUpgradeCard(m_selectedUnitId);
        }
        hideRadialMenu();
        m_mode = InteractionMode::NONE;
    });

    connect(m_btnMove, &QPushButton::clicked, this, [this]() {
        if (m_selectedUnitId >= 0) {
            m_mode = InteractionMode::MOVING;
            for (const auto &unit : m_snapshot.units) {
                if (unit.id == m_selectedUnitId) {
                    m_moveRange = unit.moveLimit;
                    break;
                }
            }
            hideRadialMenu();
            update();
        }
    });

    connect(m_btnRetreat, &QPushButton::clicked, this, [this]() {
        if (m_selectedUnitId >= 0) {
            emit signalRecallCard(m_selectedUnitId);
        }
        hideRadialMenu();
        m_mode = InteractionMode::NONE;
    });

    this->setMouseTracking(true);

    // 动画帧定时器（用于脉冲/旋转效果，独立于游戏逻辑）
    QTimer *animTimer = new QTimer(this);
    connect(animTimer, &QTimer::timeout, this, [this]() {
        m_animFrame++;
        update();
    });
    animTimer->start(50);  // 20 FPS 动画
}

// ========== setMapSize() —— 设置地图大小 ==========
void BattleView::setMapSize(int rows, int cols)
{
    m_mapRows = rows;
    m_mapCols = cols;
    this->setFixedSize(cols * CELL_SIZE, rows * CELL_SIZE);
}

// ========== updateFromSnapshot() —— 从快照更新渲染数据 ==========
void BattleView::updateFromSnapshot(const game::core::BattleSnapshot &snapshot)
{
    m_snapshot = snapshot;

    // 如果快照中有地图数据，更新地图大小
    if (snapshot.map.rows > 0 && snapshot.map.cols > 0) {
        if (m_mapRows != snapshot.map.rows || m_mapCols != snapshot.map.cols) {
            setMapSize(snapshot.map.rows, snapshot.map.cols);
        }
    }

    // 触发重绘
    update();
}

// ========== paintEvent() —— 绘制地图和所有元素 ==========
void BattleView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    drawTerrain(painter);
    drawHoverCell(painter);
    drawSpawnMarker(painter);
    drawCoreMarker(painter);
    drawHighlights(painter);
    drawUnits(painter);
    drawMonsters(painter);
}

// ========== drawTerrain() —— 绘制地形（渐变+纹理感） ==========
void BattleView::drawTerrain(QPainter &painter)
{
    for (const auto &grid : m_snapshot.map.grids) {
        QRect cellRect(grid.col * CELL_SIZE, grid.row * CELL_SIZE, CELL_SIZE, CELL_SIZE);

        // 每种地形用渐变填充，增加质感
        switch (grid.terrain) {
        case game::core::TerrainType::Path: {
            QLinearGradient grad(cellRect.topLeft(), cellRect.bottomRight());
            grad.setColorAt(0, QColor(155, 135, 115));
            grad.setColorAt(1, QColor(125, 105, 85));
            painter.fillRect(cellRect, grad);
            // 路径纹理：小点
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(100, 80, 60, 40));
            for (int dx = 8; dx < CELL_SIZE; dx += 12) {
                for (int dy = 8; dy < CELL_SIZE; dy += 12) {
                    painter.drawEllipse(cellRect.x() + dx, cellRect.y() + dy, 2, 2);
                }
            }
            break;
        }
        case game::core::TerrainType::FlatLand: {
            QLinearGradient grad(cellRect.topLeft(), cellRect.bottomRight());
            grad.setColorAt(0, QColor(62, 95, 40));
            grad.setColorAt(1, QColor(48, 78, 28));
            painter.fillRect(cellRect, grad);
            // 草地纹理：随机小线条
            painter.setPen(QPen(QColor(80, 120, 50, 50), 1));
            for (int i = 0; i < 3; ++i) {
                int sx = cellRect.x() + 10 + i * 12;
                int sy = cellRect.bottom() - 8;
                painter.drawLine(sx, sy, sx - 2, sy - 8);
            }
            break;
        }
        case game::core::TerrainType::HighGround: {
            QLinearGradient grad(cellRect.topLeft(), cellRect.bottomRight());
            grad.setColorAt(0, QColor(95, 117, 57));
            grad.setColorAt(1, QColor(75, 97, 37));
            painter.fillRect(cellRect, grad);
            // 高台标记：右上角三角 + 箭头
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 255, 255, 50));
            QPolygon tri;
            int x = cellRect.right(), y = cellRect.top();
            tri << QPoint(x, y) << QPoint(x - 10, y) << QPoint(x, y + 10);
            painter.drawPolygon(tri);
            break;
        }
        case game::core::TerrainType::NoDeploy: {
            QLinearGradient grad(cellRect.topLeft(), cellRect.bottomRight());
            grad.setColorAt(0, QColor(55, 55, 60));
            grad.setColorAt(1, QColor(40, 40, 45));
            painter.fillRect(cellRect, grad);
            break;
        }
        }

        // 网格线（淡色）
        painter.setPen(QPen(QColor(0, 0, 0, 25), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(cellRect);
    }
}

// ========== drawSpawnMarker() —— 绘制出生点（旋转传送门） ==========
void BattleView::drawSpawnMarker(QPainter &painter)
{
    int sx = m_spawnPos.col * CELL_SIZE + CELL_SIZE / 2;
    int sy = m_spawnPos.row * CELL_SIZE + CELL_SIZE / 2;
    int r = CELL_SIZE / 2 - 2;

    painter.save();
    painter.translate(sx, sy);

    // 旋转的锥形渐变（传送门效果）
    qreal angle = m_animFrame * 6.0;  // 每帧旋转6度
    QConicalGradient conGrad(0, 0, angle);
    conGrad.setColorAt(0, QColor(0, 180, 255, 120));
    conGrad.setColorAt(0.25, QColor(0, 120, 255, 40));
    conGrad.setColorAt(0.5, QColor(0, 180, 255, 120));
    conGrad.setColorAt(0.75, QColor(0, 120, 255, 40));
    conGrad.setColorAt(1, QColor(0, 180, 255, 120));

    painter.setPen(QPen(QColor(0, 200, 255, 180), 2));
    painter.setBrush(conGrad);
    painter.drawEllipse(-r, -r, r * 2, r * 2);

    // 内环
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 150, 255, 30));
    painter.drawEllipse(-r + 5, -r + 5, (r - 5) * 2, (r - 5) * 2);

    // "S" 文字
    painter.setPen(QColor(0, 220, 255));
    QFont spawnFont("Arial", 13, QFont::Bold);
    painter.setFont(spawnFont);
    QRect textRect(-r, -r, r * 2, r * 2);
    painter.drawText(textRect, Qt::AlignCenter, "S");

    // 外围发光
    QRadialGradient glow(0, 0, r + 6);
    qreal pulse = 0.3 + 0.2 * qSin(m_animFrame * 0.15);
    glow.setColorAt(0, QColor(0, 180, 255, 0));
    glow.setColorAt(0.7, QColor(0, 180, 255, 0));
    glow.setColorAt(1, QColor(0, 180, 255, int(pulse * 80)));
    painter.setPen(Qt::NoPen);
    painter.setBrush(glow);
    painter.drawEllipse(-r - 6, -r - 6, (r + 6) * 2, (r + 6) * 2);

    painter.restore();
}

// ========== drawCoreMarker() —— 绘制核心（脉冲发光） ==========
void BattleView::drawCoreMarker(QPainter &painter)
{
    int cx = m_corePos.col * CELL_SIZE + CELL_SIZE / 2;
    int cy = m_corePos.row * CELL_SIZE + CELL_SIZE / 2;
    int r = CELL_SIZE / 2 - 2;

    // 脉冲大小
    qreal pulseScale = 1.0 + 0.08 * qSin(m_animFrame * 0.12);
    int pr = static_cast<int>(r * pulseScale);

    // 外围脉冲光晕
    QRadialGradient outerGlow(cx, cy, pr + 10);
    qreal pulseAlpha = 0.3 + 0.3 * qSin(m_animFrame * 0.12);
    outerGlow.setColorAt(0, QColor(0, 212, 255, int(pulseAlpha * 100)));
    outerGlow.setColorAt(0.5, QColor(0, 180, 255, int(pulseAlpha * 40)));
    outerGlow.setColorAt(1, QColor(0, 150, 255, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(outerGlow);
    painter.drawEllipse(cx - pr - 10, cy - pr - 10, (pr + 10) * 2, (pr + 10) * 2);

    // 蓝白圆环
    QLinearGradient ringGrad(cx - pr, cy - pr, cx + pr, cy + pr);
    ringGrad.setColorAt(0, QColor(144, 202, 249));
    ringGrad.setColorAt(0.5, QColor(66, 165, 245));
    ringGrad.setColorAt(1, QColor(30, 136, 229));
    painter.setPen(QPen(QBrush(ringGrad), 3));
    painter.setBrush(QColor(0, 180, 255, 40));
    painter.drawEllipse(cx - pr, cy - pr, pr * 2, pr * 2);

    // "C" 文字
    painter.setPen(QColor(200, 230, 255));
    QFont coreFont("Arial", 13, QFont::Bold);
    painter.setFont(coreFont);
    QRect textRect(cx - r, cy - r, r * 2, r * 2);
    painter.drawText(textRect, Qt::AlignCenter, "C");
}

// ========== drawHighlights() —— 绘制部署/移动高亮 ==========
void BattleView::drawHighlights(QPainter &painter)
{
    if (m_mode == InteractionMode::DEPLOYING) {
        QVector<game::core::MapPosition> deployable = getDeployableCells();
        qreal pulse = 0.5 + 0.3 * qSin(m_animFrame * 0.15);
        for (const auto &pos : deployable) {
            QRect r(pos.col * CELL_SIZE, pos.row * CELL_SIZE, CELL_SIZE, CELL_SIZE);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 255, 100, int(pulse * 60)));
            painter.fillRect(r, painter.brush());
            // 边框
            painter.setPen(QPen(QColor(0, 255, 100, int(pulse * 120)), 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(r.adjusted(1, 1, -1, -1));
        }
    } else if (m_mode == InteractionMode::MOVING) {
        QVector<game::core::MapPosition> movable = getMovableCells(m_selectedUnitId);
        qreal pulse = 0.5 + 0.3 * qSin(m_animFrame * 0.15);
        for (const auto &pos : movable) {
            QRect r(pos.col * CELL_SIZE, pos.row * CELL_SIZE, CELL_SIZE, CELL_SIZE);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 150, 255, int(pulse * 60)));
            painter.fillRect(r, painter.brush());
            painter.setPen(QPen(QColor(0, 150, 255, int(pulse * 120)), 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(r.adjusted(1, 1, -1, -1));
        }
    }
}

// ========== drawUnits() —— 绘制单位（阴影+发光） ==========
void BattleView::drawUnits(QPainter &painter)
{
    for (const auto &unit : m_snapshot.units) {
        QRect unitRect(unit.col * CELL_SIZE, unit.row * CELL_SIZE, CELL_SIZE, CELL_SIZE);
        QRect innerRect = unitRect.adjusted(4, 4, -4, -4);

        // 根据等级选择颜色
        QColor unitColor;
        QColor glowColor;
        switch (unit.level) {
        case 1: unitColor = QColor(100, 200, 100); glowColor = QColor(100, 255, 100, 30); break;
        case 2: unitColor = QColor(100, 150, 255); glowColor = QColor(100, 150, 255, 30); break;
        case 3: unitColor = QColor(255, 180, 50);  glowColor = QColor(255, 200, 50, 40);  break;
        }

        // 底部阴影
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 40));
        painter.drawRoundedRect(innerRect.adjusted(2, 3, 2, 3), 6, 6);

        // 单位方块（渐变填充）
        QLinearGradient unitGrad(innerRect.topLeft(), innerRect.bottomRight());
        unitGrad.setColorAt(0, unitColor.lighter(120));
        unitGrad.setColorAt(1, unitColor.darker(110));
        painter.setPen(QPen(QColor(255, 255, 255, 160), 1));
        painter.setBrush(unitGrad);
        painter.drawRoundedRect(innerRect, 6, 6);

        // 选中发光效果
        if (unit.id == m_selectedUnitId && m_mode == InteractionMode::RADIAL_MENU) {
            qreal pulse = 0.6 + 0.4 * qSin(m_animFrame * 0.2);
            painter.setPen(QPen(QColor(0, 212, 255, int(pulse * 200)), 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(innerRect.adjusted(-1, -1, 1, 1), 7, 7);

            // 发光光晕
            QRadialGradient glowGrad(innerRect.center(), CELL_SIZE / 2);
            glowGrad.setColorAt(0, QColor(0, 212, 255, int(pulse * 50)));
            glowGrad.setColorAt(1, QColor(0, 212, 255, 0));
            painter.setPen(Qt::NoPen);
            painter.setBrush(glowGrad);
            painter.drawEllipse(innerRect.center(), CELL_SIZE / 2, CELL_SIZE / 2);
        }

        // 等级标识
        painter.setPen(QColor(255, 255, 255, 220));
        QFont unitFont("Arial", 9, QFont::Bold);
        painter.setFont(unitFont);
        painter.drawText(innerRect.adjusted(2, 1, 0, 0),
                         Qt::AlignTop | Qt::AlignLeft,
                         QString("Lv%1").arg(unit.level));

        // 血量条（渐变）
        int barWidth = CELL_SIZE - 10;
        int barHeight = 4;
        int barX = innerRect.x() + 1;
        int barY = innerRect.bottom() - 7;

        // 血量条背景
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(30, 30, 30, 180));
        painter.drawRoundedRect(barX, barY, barWidth, barHeight, 2, 2);

        // 血量条填充
        double hpRatio = (unit.maxHp > 0) ? static_cast<double>(unit.hp) / unit.maxHp : 0.0;
        int hpWidth = static_cast<int>(barWidth * hpRatio);
        if (hpWidth > 0) {
            QLinearGradient hpGrad(barX, barY, barX + hpWidth, barY);
            if (hpRatio > 0.5) {
                hpGrad.setColorAt(0, QColor(80, 220, 80));
                hpGrad.setColorAt(1, QColor(50, 180, 50));
            } else if (hpRatio > 0.25) {
                hpGrad.setColorAt(0, QColor(255, 200, 50));
                hpGrad.setColorAt(1, QColor(220, 160, 30));
            } else {
                hpGrad.setColorAt(0, QColor(255, 80, 80));
                hpGrad.setColorAt(1, QColor(200, 40, 40));
            }
            painter.setBrush(hpGrad);
            painter.drawRoundedRect(barX, barY, hpWidth, barHeight, 2, 2);
        }

        // 攻击范围指示（选中时）
        if (unit.id == m_selectedUnitId) {
            int centerX = unit.col * CELL_SIZE + CELL_SIZE / 2;
            int centerY = unit.row * CELL_SIZE + CELL_SIZE / 2;
            int rangeRadius = unit.range * CELL_SIZE;
            qreal pulse = 0.5 + 0.3 * qSin(m_animFrame * 0.1);
            painter.setPen(QPen(QColor(0, 212, 255, int(pulse * 100)), 1, Qt::DashLine));
            painter.setBrush(QColor(0, 212, 255, int(pulse * 12)));
            painter.drawEllipse(QPoint(centerX, centerY), rangeRadius, rangeRadius);
        }
    }
}

// ========== drawMonsters() —— 绘制怪物 ==========
void BattleView::drawMonsters(QPainter &painter)
{
    for (const auto &monster : m_snapshot.monsters) {
        QRect mRect(monster.col * CELL_SIZE, monster.row * CELL_SIZE, CELL_SIZE, CELL_SIZE);
        QRect innerRect = mRect.adjusted(6, 6, -6, -6);

        // 底部阴影
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 50));
        painter.drawRoundedRect(innerRect.adjusted(2, 2, 2, 2), 4, 4);

        // 怪物方块（红色渐变）
        QLinearGradient monsterGrad(innerRect.topLeft(), innerRect.bottomRight());
        monsterGrad.setColorAt(0, QColor(240, 70, 70));
        monsterGrad.setColorAt(1, QColor(180, 30, 30));
        painter.setPen(QPen(QColor(255, 100, 100, 120), 1));
        painter.setBrush(monsterGrad);
        painter.drawRoundedRect(innerRect, 4, 4);

        // 怪物血量条
        int barWidth = CELL_SIZE - 16;
        int barHeight = 3;
        int barX = innerRect.x();
        int barY = innerRect.bottom() - 5;

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(40, 10, 10, 180));
        painter.drawRoundedRect(barX, barY, barWidth, barHeight, 1, 1);

        double hpRatio = (monster.maxHp > 0) ? static_cast<double>(monster.hp) / monster.maxHp : 0.0;
        int hpWidth = static_cast<int>(barWidth * hpRatio);
        if (hpWidth > 0) {
            QLinearGradient hpGrad(barX, barY, barX + hpWidth, barY);
            hpGrad.setColorAt(0, QColor(255, 100, 100));
            hpGrad.setColorAt(1, QColor(200, 50, 50));
            painter.setBrush(hpGrad);
            painter.drawRoundedRect(barX, barY, hpWidth, barHeight, 1, 1);
        }
    }
}

// ========== drawHoverCell() —— 绘制悬停格子高亮 ==========
void BattleView::drawHoverCell(QPainter &painter)
{
    if (m_hoverRow < 0 || m_hoverCol < 0) return;
    if (m_hoverRow >= m_mapRows || m_hoverCol >= m_mapCols) return;

    QRect hoverRect(m_hoverCol * CELL_SIZE, m_hoverRow * CELL_SIZE, CELL_SIZE, CELL_SIZE);
    painter.setPen(QPen(QColor(0, 212, 255, 70), 2));
    painter.setBrush(QColor(0, 212, 255, 18));
    painter.drawRect(hoverRect);
}

// ========== mouseMoveEvent() —— 鼠标移动追踪 ==========
void BattleView::mouseMoveEvent(QMouseEvent *event)
{
    int col = event->pos().x() / CELL_SIZE;
    int row = event->pos().y() / CELL_SIZE;

    if (row != m_hoverRow || col != m_hoverCol) {
        m_hoverRow = row;
        m_hoverCol = col;
        update();
    }
}

// ========== mousePressEvent() —— 处理鼠标点击 ==========
void BattleView::mousePressEvent(QMouseEvent *event)
{
    // 像素坐标 → 网格坐标
    int col = event->pos().x() / CELL_SIZE;
    int row = event->pos().y() / CELL_SIZE;

    // 边界检查
    if (row < 0 || row >= m_mapRows || col < 0 || col >= m_mapCols) return;

    switch (m_mode) {
    case InteractionMode::NONE: {
        // 检查是否点击了己方单位
        int unitId = findUnitAt(row, col);
        if (unitId >= 0) {
            m_selectedUnitId = unitId;
            m_mode = InteractionMode::RADIAL_MENU;
            showRadialMenu(unitId, col * CELL_SIZE + CELL_SIZE / 2, row * CELL_SIZE + CELL_SIZE / 2);
            update();
        }
        break;
    }

    case InteractionMode::DEPLOYING: {
        // 检查目标格是否可部署
        bool canDeploy = false;
        for (const auto &grid : m_snapshot.map.grids) {
            if (grid.row == row && grid.col == col &&
                !grid.occupied &&
                (grid.terrain == game::core::TerrainType::FlatLand ||
                 grid.terrain == game::core::TerrainType::HighGround)) {
                canDeploy = true;
                break;
            }
        }
        if (canDeploy) {
            // 发出部署信号 → BattlePage 调用 BattleManager::deployCard
            emit signalDeployCard(m_selectedCardKind, game::core::MapPosition(row, col));
            m_mode = InteractionMode::NONE;
            update();
        }
        // 右键取消部署
        if (event->button() == Qt::RightButton) {
            m_mode = InteractionMode::NONE;
            update();
        }
        break;
    }

    case InteractionMode::MOVING: {
        QVector<game::core::MapPosition> movable = getMovableCells(m_selectedUnitId);
        bool isValid = false;
        for (const auto &pos : movable) {
            if (pos.row == row && pos.col == col) { isValid = true; break; }
        }
        if (isValid) {
            emit signalMoveCard(m_selectedUnitId, game::core::MapPosition(row, col));
            m_mode = InteractionMode::NONE;
            m_selectedUnitId = -1;
            update();
        } else if (event->button() == Qt::RightButton) {
            m_mode = InteractionMode::NONE;
            m_selectedUnitId = -1;
            update();
        }
        break;
    }

    case InteractionMode::RADIAL_MENU: {
        // 点击非菜单区域 → 关闭菜单
        if (!m_btnUpgrade->geometry().contains(event->pos()) &&
            !m_btnMove->geometry().contains(event->pos()) &&
            !m_btnRetreat->geometry().contains(event->pos())) {
            hideRadialMenu();
            m_mode = InteractionMode::NONE;
            m_selectedUnitId = -1;
            update();
        }
        break;
    }
    }
}

// ========== 辅助方法实现 ==========

void BattleView::showRadialMenu(int unitId, int pixelX, int pixelY)
{
    // 从快照查找单位信息
    int level = 1;
    for (const auto &unit : m_snapshot.units) {
        if (unit.id == unitId) { level = unit.level; break; }
    }

    int btnWidth = 70, btnHeight = 36;

    m_btnUpgrade->setGeometry(pixelX - btnWidth / 2, pixelY - CELL_SIZE - btnHeight - 5, btnWidth, btnHeight);
    m_btnMove->setGeometry(pixelX - CELL_SIZE - btnWidth - 5, pixelY + 10, btnWidth, btnHeight);
    m_btnRetreat->setGeometry(pixelX + CELL_SIZE + 5, pixelY + 10, btnWidth, btnHeight);

    // 升级按钮状态
    m_btnUpgrade->setEnabled(level < game::core::constants::MaxCardLevel);
    m_btnUpgrade->setText(level >= game::core::constants::MaxCardLevel ?
                              "⬆ 满级" : QString("⬆ Lv%1→%2").arg(level).arg(level + 1));

    m_btnUpgrade->show();
    m_btnMove->show();
    m_btnRetreat->show();
}

void BattleView::hideRadialMenu()
{
    m_btnUpgrade->hide();
    m_btnMove->hide();
    m_btnRetreat->hide();
}

int BattleView::findUnitAt(int row, int col) const
{
    for (const auto &unit : m_snapshot.units) {
        if (unit.row == row && unit.col == col) return unit.id;
    }
    return -1;
}

QVector<game::core::MapPosition> BattleView::getDeployableCells() const
{
    QVector<game::core::MapPosition> result;
    for (const auto &grid : m_snapshot.map.grids) {
        if (!grid.occupied &&
            (grid.terrain == game::core::TerrainType::FlatLand ||
             grid.terrain == game::core::TerrainType::HighGround)) {
            result.append(game::core::MapPosition(grid.row, grid.col));
        }
    }
    return result;
}

QVector<game::core::MapPosition> BattleView::getMovableCells(int unitId) const
{
    QVector<game::core::MapPosition> result;

    // 从快照查找单位的当前位置和移动范围
    game::core::MapPosition unitPos;
    bool found = false;
    for (const auto &unit : m_snapshot.units) {
        if (unit.id == unitId) {
            unitPos = game::core::MapPosition(unit.row, unit.col);
            found = true;
            break;
        }
    }
    if (!found) return result;

    // 曼哈顿距离不超过 moveRange 的可部署空格
    for (const auto &grid : m_snapshot.map.grids) {
        game::core::MapPosition gridPos(grid.row, grid.col);
        int dist = unitPos.manhattanDistanceTo(gridPos);
        if (dist > 0 && dist <= m_moveRange && !grid.occupied &&
            (grid.terrain == game::core::TerrainType::FlatLand ||
             grid.terrain == game::core::TerrainType::HighGround)) {
            result.append(gridPos);
        }
    }
    return result;
}


// ============================================================================
// BattlePage 实现
// ============================================================================

BattlePage::BattlePage(QWidget *parent)
    : QWidget(parent)
    , m_battleView(nullptr)
    , m_gameTimer(nullptr)
    , m_waveLabel(nullptr)
    , m_coreHpLabel(nullptr)
    , m_resourceLabel(nullptr)
    , m_btnPause(nullptr)
    , m_btnSpeed(nullptr)
    , m_btnSkill(nullptr)
    , m_isPaused(false)
    , m_speedMultiplier(1.0)
    , m_battleManager(nullptr)
{
    initUI();
    connectSignals();
}

// ========== initUI() —— 初始化界面 ==========
void BattlePage::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ===== 顶部状态栏 =====
    {
        QWidget *barContainer = new QWidget(this);
        barContainer->setFixedHeight(50);
        barContainer->setStyleSheet(
            "QWidget {"
            "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "    stop:0 rgba(10,20,38,0.96), stop:1 rgba(14,28,52,0.92));"
            "  border-bottom: 2px solid rgba(0,212,255,0.40);"
            "}"
        );
        QHBoxLayout *layout = new QHBoxLayout(barContainer);
        layout->setContentsMargins(20, 0, 20, 0);

        m_waveLabel = new QLabel("🌊 波次: 0", barContainer);
        m_waveLabel->setStyleSheet("color: #FFFFFF; font-size: 16px; font-weight: bold; background: transparent;");

        m_coreHpLabel = new QLabel("🏰 核心: 10", barContainer);
        m_coreHpLabel->setStyleSheet("color: #00E5FF; font-size: 16px; font-weight: bold; background: transparent;");

        m_resourceLabel = new QLabel("💰 资源: 100", barContainer);
        m_resourceLabel->setStyleSheet("color: #FFD54F; font-size: 16px; font-weight: bold; background: transparent;");

        layout->addWidget(m_waveLabel);
        layout->addStretch();
        layout->addWidget(m_coreHpLabel);
        layout->addStretch();
        layout->addWidget(m_resourceLabel);
        mainLayout->addWidget(barContainer);
    }

    // ===== 中央战斗视口 =====
    m_battleView = new BattleView(this);
    QHBoxLayout *viewLayout = new QHBoxLayout();
    viewLayout->addStretch();
    viewLayout->addWidget(m_battleView);
    viewLayout->addStretch();
    mainLayout->addLayout(viewLayout, 1);

    // ===== 底部操作栏 =====
    {
        QWidget *barContainer = new QWidget(this);
        barContainer->setFixedHeight(90);
        barContainer->setStyleSheet(
            "QWidget {"
            "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "    stop:0 rgba(14,28,52,0.92), stop:1 rgba(10,20,38,0.96));"
            "  border-top: 2px solid rgba(0,212,255,0.40);"
            "}"
        );
        QHBoxLayout *layout = new QHBoxLayout(barContainer);
        layout->setContentsMargins(15, 10, 15, 10);

        // 卡牌按钮
        QStringList cardNames = {"突击手", "采矿工", "医生", "狙击手", "AOE炮塔"};
        QVector<game::core::CardKind> cardKinds = {
            game::core::CardKind::Attack,
            game::core::CardKind::Produce,
            game::core::CardKind::Heal,
            game::core::CardKind::Attack,
            game::core::CardKind::Attack
        };
        QStringList cardCosts = {"40", "35", "30", "50", "60"};
        QStringList cardIcons = {"⚔", "⛏", "❤", "🎯", "💥"};

        for (int i = 0; i < cardNames.size(); ++i) {
            QPushButton *cardBtn = new QPushButton(barContainer);
            cardBtn->setFixedSize(90, 70);
            cardBtn->setText(QString("%1\n%2\n💰%3")
                                 .arg(cardIcons[i])
                                 .arg(cardNames[i])
                                 .arg(cardCosts[i]));
            cardBtn->setStyleSheet(
                "QPushButton {"
                "  background-color: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                "    stop:0 rgba(22,50,90,0.90), stop:1 rgba(12,30,55,0.85));"
                "  color: #FFFFFF;"
                "  border: 2px solid rgba(0,212,255,0.55); border-radius: 10px;"
                "  font-size: 11px; font-weight: bold;"
                "}"
                "QPushButton:hover {"
                "  border: 2px solid #00D4FF;"
                "  background-color: rgba(0,212,255,0.18);"
                "  color: #00E5FF;"
                "}"
                "QPushButton:pressed { background-color: rgba(0,212,255,0.30); }"
            );
            cardBtn->setCursor(Qt::PointingHandCursor);

            game::core::CardKind kind = cardKinds[i];
            connect(cardBtn, &QPushButton::clicked, this, [this, kind]() {
                m_battleView->m_mode = BattleView::InteractionMode::DEPLOYING;
                m_battleView->m_selectedCardKind = kind;
                m_battleView->hideRadialMenu();
                m_battleView->update();
            });

            m_cardButtons.append(cardBtn);
            layout->addWidget(cardBtn);
        }

        layout->addSpacing(20);

        // 暂停按钮
        m_btnPause = new QPushButton("⏸", barContainer);
        m_btnPause->setFixedSize(50, 45);
        m_btnPause->setStyleSheet(
            "QPushButton { background-color: rgba(22,50,90,0.85); color: #FFFFFF;"
            "  border: 2px solid rgba(0,212,255,0.50); border-radius: 10px; font-size: 16px; }"
            "QPushButton:hover { color: #00E5FF; border: 2px solid #00D4FF; }"
        );
        layout->addWidget(m_btnPause);

        // 加速按钮
        m_btnSpeed = new QPushButton("1x", barContainer);
        m_btnSpeed->setFixedSize(50, 45);
        m_btnSpeed->setStyleSheet(m_btnPause->styleSheet());
        layout->addWidget(m_btnSpeed);

        // 技能按钮
        m_btnSkill = new QPushButton("⚡", barContainer);
        m_btnSkill->setFixedSize(50, 45);
        m_btnSkill->setEnabled(false);
        m_btnSkill->setStyleSheet(
            "QPushButton { background-color: rgba(22,40,70,0.60); color: #7AACCC;"
            "  border: 2px solid rgba(0,212,255,0.25); border-radius: 10px; font-size: 16px; }"
        );
        m_btnSkill->setToolTip("技能为自动释放");
        layout->addWidget(m_btnSkill);

        mainLayout->addWidget(barContainer);
    }

    // 页面背景
    this->setStyleSheet("BattlePage { background-color: #0B1622; }");

    // 游戏主循环定时器（约 60FPS）
    m_gameTimer = new QTimer(this);
    connect(m_gameTimer, &QTimer::timeout, this, &BattlePage::onGameTick);
}

// ========== startBattle() —— 开始战斗 ==========
void BattlePage::startBattle()
{
    // 获取 BattleManager 引用（通过 MainWindow）
    MainWindow *mainWin = qobject_cast<MainWindow*>(window());
    if (mainWin) {
        m_battleManager = mainWin->battleManager();
    }

    if (!m_battleManager) return;

    // ----- 初始化地图地形 -----
    auto& map = m_battleManager->map();

    // 定义出生点和核心位置
    game::core::MapPosition spawnPos(1, 1);
    game::core::MapPosition corePos(10, 16);

    // 1) 设置 NoDeploy 边框
    for (int r = 0; r < map.rows(); ++r) {
        map.setGrid({r, 0}, game::core::TerrainType::NoDeploy, 0);
        map.setGrid({r, map.cols() - 1}, game::core::TerrainType::NoDeploy, 0);
    }
    for (int c = 0; c < map.cols(); ++c) {
        map.setGrid({0, c}, game::core::TerrainType::NoDeploy, 0);
        map.setGrid({map.rows() - 1, c}, game::core::TerrainType::NoDeploy, 0);
    }

    // 2) 定义 S 型路径（出生点 → 核心）
    std::vector<game::core::MapPosition> pathCells = {
        // 第1段：向右
        {1,1}, {1,2}, {1,3}, {1,4}, {1,5}, {1,6},
        // 第2段：向下
        {2,6}, {3,6},
        // 第3段：向右
        {4,6}, {4,7}, {4,8}, {4,9}, {4,10}, {4,11}, {4,12}, {4,13},
        // 第4段：向下
        {5,13}, {6,13},
        // 第5段：向左
        {7,13}, {7,12}, {7,11}, {7,10}, {7,9}, {7,8}, {7,7}, {7,6},
        // 第6段：向下
        {8,6}, {9,6},
        // 第7段：向右到核心
        {10,6}, {10,7}, {10,8}, {10,9}, {10,10}, {10,11},
        {10,12}, {10,13}, {10,14}, {10,15}, {10,16}
    };

    // 3) 标记路径格为 Path 地形（可走不可部署）
    for (const auto& pos : pathCells) {
        map.setGrid(pos, game::core::TerrainType::Path, 0);
    }

    // 4) 设置高台（战略要地，射程+1）
    std::vector<game::core::MapPosition> highGroundCells = {
        // 第1段弯道旁
        {2,2}, {2,3}, {3,2}, {3,3},
        {2,8}, {2,9}, {3,8}, {3,9},
        // 第3段弯道旁
        {5,5}, {5,6}, {6,5}, {6,6},
        {5,10}, {5,11}, {6,10}, {6,11},
        // 第5段弯道旁
        {8,9}, {8,10}, {9,9}, {9,10},
        {8,14}, {8,15}, {9,14}, {9,15}
    };
    for (const auto& pos : highGroundCells) {
        map.setGrid(pos, game::core::TerrainType::HighGround, 1);
    }

    // 5) 设置出生点和路径
    m_battleManager->setSpawnPoint(spawnPos);
    m_battleManager->setPath(pathCells);

    // 记录给 BattleView 用于渲染标记
    m_battleView->m_spawnPos = spawnPos;
    m_battleView->m_corePos = corePos;

    // ----- 启动第一波怪物 -----
    m_currentWaveId = 1;
    m_battleManager->startWave(m_currentWaveId);

    // 重置状态
    m_isPaused = false;
    m_speedMultiplier = 1.0;
    m_waveTimer = 0.0;

    // 启动游戏主循环
    m_gameTimer->start(16);

    // 立即渲染一帧
    game::core::BattleSnapshot snap = m_battleManager->snapshot();
    m_battleView->updateFromSnapshot(snap);
    updateStatusBar(snap);
}

// ========== onGameTick() —— 游戏主循环回调 ==========
void BattlePage::onGameTick()
{
    if (!m_battleManager || m_isPaused) return;

    // 推进游戏逻辑（deltaSeconds = 帧间隔 * 速度倍率）
    double deltaSeconds = game::core::constants::DefaultFrameSeconds * m_speedMultiplier;
    m_battleManager->update(deltaSeconds);

    // ----- 自动推进下一波怪物 -----
    // 当当前波的所有怪物被消灭或逃逸后，经过 WAVE_INTERVAL 秒自动出下一波
    game::core::BattleSnapshot snap = m_battleManager->snapshot();
    if (snap.monsters.empty()) {
        m_waveTimer += deltaSeconds;
        if (m_waveTimer >= WAVE_INTERVAL) {
            m_waveTimer = 0.0;
            m_currentWaveId++;
            m_battleManager->startWave(m_currentWaveId);
            snap = m_battleManager->snapshot();  // 刷新快照
        }
    } else {
        m_waveTimer = 0.0;  // 场上还有怪时重置计时器
    }

    // 获取快照并更新界面
    m_battleView->updateFromSnapshot(snap);
    updateStatusBar(snap);

    // 检查游戏是否结束
    if (snap.gameOver) {
        m_gameTimer->stop();
        emit signalBattleEnd();
    }
}

// ========== updateStatusBar() —— 更新状态栏 ==========
void BattlePage::updateStatusBar(const game::core::BattleSnapshot &snapshot)
{
    m_waveLabel->setText(QString("🌊 波次: %1").arg(snapshot.currentWave));
    m_coreHpLabel->setText(QString("🏰 核心: %1").arg(snapshot.baseHealth));
    m_resourceLabel->setText(QString("💰 资源: %1").arg(snapshot.resources));
}

// ========== connectSignals() —— 连接信号槽 ==========
void BattlePage::connectSignals()
{
    // 暂停按钮
    connect(m_btnPause, &QPushButton::clicked, this, [this]() {
        m_isPaused = !m_isPaused;
        m_btnPause->setText(m_isPaused ? "▶" : "⏸");
    });

    // 加速按钮
    connect(m_btnSpeed, &QPushButton::clicked, this, [this]() {
        m_speedMultiplier = (m_speedMultiplier == 1.0) ? 2.0 : 1.0;
        m_btnSpeed->setText(m_speedMultiplier == 2.0 ? "2x" : "1x");
    });

    // ===== BattleView 的操作信号 → 调用 BattleManager 接口 =====

    // 部署卡牌
    connect(m_battleView, &BattleView::signalDeployCard,
            this, [this](game::core::CardKind kind, game::core::MapPosition pos) {
        if (m_battleManager) {
            m_battleManager->deployCard(kind, pos);
        }
    });

    // 升级单位
    connect(m_battleView, &BattleView::signalUpgradeCard,
            this, [this](int unitId) {
        if (m_battleManager) {
            m_battleManager->upgradeCard(unitId);
        }
    });

    // 移动单位
    connect(m_battleView, &BattleView::signalMoveCard,
            this, [this](int unitId, game::core::MapPosition target) {
        if (m_battleManager) {
            m_battleManager->moveCard(unitId, target);
        }
    });

    // 撤回单位
    connect(m_battleView, &BattleView::signalRecallCard,
            this, [this](int unitId) {
        if (m_battleManager) {
            m_battleManager->recallCard(unitId);
        }
    });
}
