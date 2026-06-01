/**
 * @file DeployPage.cpp
 * @brief 迷雾部署页面实现
 */

#include "ui/DeployPage.h"
#include "ui/MainWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <QtEndian>

// ========== 网络模块 ==========
#include "network/session/GameServer.h"
#include "network/session/GameClient.h"

namespace {
QString cardName(game::core::CardKind kind)
{
    switch (kind) {
    case game::core::CardKind::Attack: return "突击手";
    case game::core::CardKind::Sniper: return "狙击手";
    case game::core::CardKind::Aoe: return "AOE炮塔";
    case game::core::CardKind::Specialist: return "特种兵";
    case game::core::CardKind::Produce: return "采矿工";
    case game::core::CardKind::Arsenal: return "兵工厂";
    case game::core::CardKind::Heal: return "医生";
    case game::core::CardKind::HeavyMedic: return "重装医生";
    }
    return "未知";
}

QString cardIcon(game::core::CardKind kind)
{
    if (game::core::isAttackCardKind(kind)) return "⚔";
    if (game::core::isProduceCardKind(kind)) return "⛏";
    return "❤";
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
    , m_btnUpgrade(nullptr)
    , m_btnMove(nullptr)
    , m_btnRecall(nullptr)
{
    setMapSize(m_mapRows, m_mapCols);
    setMouseTracking(true);

    m_btnUpgrade = new QPushButton("⬆ 升级", this);
    m_btnMove = new QPushButton("🏃 移动", this);
    m_btnRecall = new QPushButton("↩ 撤回", this);

    const QString radialStyle =
        "QPushButton {"
        "  background-color: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 rgba(22,50,90,0.95), stop:1 rgba(12,28,50,0.95));"
        "  color: #FFD54F;"
        "  border: 2px solid rgba(255,213,79,0.65); border-radius: 18px;"
        "  font-size: 13px; font-weight: bold; padding: 8px 14px;"
        "}"
        "QPushButton:hover { background-color: rgba(255,213,79,0.25); border: 2px solid #FFD54F; }"
        "QPushButton:disabled { color: #667788; border: 2px solid rgba(255,213,79,0.25); }";
    m_btnUpgrade->setStyleSheet(radialStyle);
    m_btnMove->setStyleSheet(radialStyle);
    m_btnRecall->setStyleSheet(radialStyle);
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
}

void DeployView::setMapSize(int rows, int cols)
{
    m_mapRows = rows;
    m_mapCols = cols;
    this->setFixedSize(cols * CELL_SIZE, rows * CELL_SIZE);
}

void DeployView::updateFromSnapshot(const game::core::BattleSnapshot &snapshot)
{
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
    drawDeployable(painter);
    drawUnits(painter);
    drawHoverCell(painter);
}

void DeployView::drawTerrain(QPainter &painter)
{
    for (const auto &grid : m_snapshot.map.grids) {
        QRect cellRect(grid.col * CELL_SIZE, grid.row * CELL_SIZE, CELL_SIZE, CELL_SIZE);

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
        painter.drawRect(cellRect);

        // 在出生点和核心上绘制标记
        if (grid.terrain == game::core::TerrainType::SpawnPoint) {
            painter.setPen(QPen(QColor(255, 255, 255), 2));
            painter.drawText(cellRect, Qt::AlignCenter, "S");
        } else if (grid.terrain == game::core::TerrainType::CoreA) {
            painter.setPen(QPen(QColor(255, 255, 255), 2));
            painter.drawText(cellRect, Qt::AlignCenter, "A");
        } else if (grid.terrain == game::core::TerrainType::CoreB) {
            painter.setPen(QPen(QColor(255, 255, 255), 2));
            painter.drawText(cellRect, Qt::AlignCenter, "B");
        }
    }
}

void DeployView::drawDeployable(QPainter &painter)
{
    if (m_mode != InteractionMode::DEPLOYING && m_mode != InteractionMode::MOVING) return;

    if (m_mode == InteractionMode::MOVING) {
        for (const auto& pos : getMovableCells(m_selectedUnitId)) {
            QRect cellRect(pos.col * CELL_SIZE, pos.row * CELL_SIZE, CELL_SIZE, CELL_SIZE);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 213, 79, 48));
            painter.drawRect(cellRect);
        }
        return;
    }

    for (const auto &grid : m_snapshot.map.grids) {
        if (!grid.occupied &&
            (grid.terrain == game::core::TerrainType::FlatLand ||
             grid.terrain == game::core::TerrainType::HighGround)) {
            QRect cellRect(grid.col * CELL_SIZE, grid.row * CELL_SIZE, CELL_SIZE, CELL_SIZE);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 255, 0, 40));
            painter.drawRect(cellRect);
        }
    }
}

void DeployView::drawUnits(QPainter &painter)
{
    for (const auto &unit : m_snapshot.units) {
        QRect unitRect(unit.col * CELL_SIZE, unit.row * CELL_SIZE, CELL_SIZE, CELL_SIZE);
        QRect innerRect = unitRect.adjusted(4, 4, -4, -4);

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
        painter.setPen(QPen(borderColor, 2));
        painter.setBrush(unitColor);
        painter.drawRoundedRect(innerRect, 6, 6);

        if (unit.id == m_selectedUnitId) {
            painter.setPen(QPen(QColor(255, 213, 79), 3));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(innerRect.adjusted(-2, -2, 2, 2), 8, 8);
        }

        // 绘制单位标签
        painter.setPen(QColor(255, 255, 255));
        QFont font("Microsoft YaHei", 10, QFont::Bold);
        painter.setFont(font);
        painter.drawText(innerRect, Qt::AlignCenter, label);
    }
}

void DeployView::drawHoverCell(QPainter &painter)
{
    if (m_hoverRow < 0 || m_hoverCol < 0) return;
    if (m_hoverRow >= m_mapRows || m_hoverCol >= m_mapCols) return;

    QRect hoverRect(m_hoverCol * CELL_SIZE, m_hoverRow * CELL_SIZE, CELL_SIZE, CELL_SIZE);
    painter.setPen(QPen(QColor(0, 212, 255, 70), 2));
    painter.setBrush(QColor(0, 212, 255, 18));
    painter.drawRect(hoverRect);
}

void DeployView::mouseMoveEvent(QMouseEvent *event)
{
    int col = event->pos().x() / CELL_SIZE;
    int row = event->pos().y() / CELL_SIZE;

    if (row != m_hoverRow || col != m_hoverCol) {
        m_hoverRow = row;
        m_hoverCol = col;
        update();
    }
}

void DeployView::mousePressEvent(QMouseEvent *event)
{
    int col = event->pos().x() / CELL_SIZE;
    int row = event->pos().y() / CELL_SIZE;

    if (row < 0 || row >= m_mapRows || col < 0 || col >= m_mapCols) return;

    if (m_mode == InteractionMode::NONE) {
        int unitId = findOwnUnitAt(row, col);
        if (unitId > 0) {
            m_selectedUnitId = unitId;
            m_mode = InteractionMode::RADIAL_MENU;
            showRadialMenu(unitId, col * CELL_SIZE + CELL_SIZE / 2, row * CELL_SIZE + CELL_SIZE / 2);
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

    const int btnWidth = 70;
    const int btnHeight = 36;
    m_btnUpgrade->setGeometry(pixelX - btnWidth / 2, pixelY - CELL_SIZE - btnHeight - 5, btnWidth, btnHeight);
    m_btnMove->setGeometry(pixelX - CELL_SIZE - btnWidth - 5, pixelY + 10, btnWidth, btnHeight);
    m_btnRecall->setGeometry(pixelX + CELL_SIZE + 5, pixelY + 10, btnWidth, btnHeight);

    m_btnUpgrade->setEnabled(level < game::core::constants::MaxCardLevel);
    m_btnUpgrade->setText(level >= game::core::constants::MaxCardLevel
                              ? "⬆ 满级"
                              : QString("⬆ Lv%1→%2").arg(level).arg(level + 1));

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
    , m_deployCountLabel(nullptr)
    , m_btnBack(nullptr)
    , m_btnStartBattle(nullptr)
    , m_isPvp(false)
    , m_isHost(false)
    , m_battleManager(nullptr)
    , m_deployedCount(0)
    , m_selectedUnitId(-1)
    , m_localReady(false)
    , m_opponentReady(false)
    , m_opponentLabel(nullptr)
{
    initUI();
    connectSignals();
}

void DeployPage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    static QPixmap bg(":/images/ui/scene_lab_02.png");
    if (!bg.isNull()) {
        QSize scaled = bg.size();
        scaled.scale(size(), Qt::KeepAspectRatioByExpanding);
        QRect target(QPoint((width() - scaled.width()) / 2,
                            (height() - scaled.height()) / 2), scaled);
        painter.drawPixmap(target, bg);
    } else {
        painter.fillRect(rect(), QColor(37, 30, 34));
    }
    painter.fillRect(rect(), QColor(35, 24, 21, 118));
}

void DeployPage::setNetworkContext(const NetworkContext& ctx)
{
    m_netCtx = ctx;
    m_isPvp = ctx.isPvp;
    m_isHost = ctx.isHost;
}

void DeployPage::setDeck(const QVector<game::core::CardKind>& deck)
{
    m_deck = deck;
}

void DeployPage::initUI()
{
    setAutoFillBackground(false);
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
            "    stop:0 rgba(73,50,39,0.96), stop:1 rgba(43,31,28,0.94));"
            "  border-bottom: 2px solid rgba(255,210,126,0.42);"
            "}"
        );
        QHBoxLayout *layout = new QHBoxLayout(barContainer);
        layout->setContentsMargins(20, 0, 20, 0);

        m_btnBack = new QPushButton("← 返回", barContainer);
        m_btnBack->setFixedSize(80, 36);
        m_btnBack->setStyleSheet(
            "QPushButton { background-color: rgba(225,176,99,0.86); color: #3A2418;"
            "  border: 2px solid rgba(76,48,31,0.82); border-radius: 8px; font-size: 14px; font-weight: bold; }"
            "QPushButton:hover { border: 2px solid #FFD27E; }"
        );
        m_btnBack->setCursor(Qt::PointingHandCursor);

        m_titleLabel = new QLabel("迷雾部署阶段", barContainer);
        m_titleLabel->setStyleSheet("color: #FFF0C8; font-size: 18px; font-weight: bold;");

        m_deployCountLabel = new QLabel("已部署: 0 个单位", barContainer);
        m_deployCountLabel->setStyleSheet("color: #FFD54F; font-size: 16px; font-weight: bold;");

        m_opponentLabel = new QLabel("对手: 等待中...", barContainer);
        m_opponentLabel->setStyleSheet("color: #9EE0C7; font-size: 16px;");

        layout->addWidget(m_btnBack);
        layout->addSpacing(20);
        layout->addWidget(m_titleLabel);
        layout->addStretch();
        layout->addWidget(m_deployCountLabel);
        layout->addSpacing(20);
        layout->addWidget(m_opponentLabel);

        mainLayout->addWidget(barContainer);
    }

    // ===== 中央部署视口 =====
    m_deployView = new DeployView(this);
    QHBoxLayout *viewLayout = new QHBoxLayout();
    viewLayout->addStretch();
    viewLayout->addWidget(m_deployView);
    viewLayout->addStretch();
    mainLayout->addLayout(viewLayout, 1);

    // ===== 底部操作栏 =====
    {
        QWidget *barContainer = new QWidget(this);
        barContainer->setFixedHeight(90);
        barContainer->setStyleSheet(
            "QWidget {"
            "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "    stop:0 rgba(43,31,28,0.92), stop:1 rgba(73,50,39,0.96));"
            "  border-top: 2px solid rgba(255,210,126,0.42);"
            "}"
        );
        QHBoxLayout *layout = new QHBoxLayout(barContainer);
        layout->setContentsMargins(15, 10, 15, 10);

        // 卡牌按钮（5个槽位，将在 initDeployment 中根据实际选卡更新）

        for (int i = 0; i < 5; ++i) {
            QPushButton *cardBtn = new QPushButton(barContainer);
            cardBtn->setFixedSize(100, 70);
            cardBtn->setText("空");
            cardBtn->setStyleSheet(
                "QPushButton {"
                "  background-color: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                "    stop:0 rgba(225,176,99,0.95), stop:1 rgba(123,82,50,0.92));"
                "  color: #3A2418;"
                "  border: 2px solid rgba(76,48,31,0.82); border-radius: 8px;"
                "  font-size: 11px; font-weight: bold;"
                "}"
                "QPushButton:hover {"
                "  border: 2px solid #FFD27E;"
                "  background-color: rgba(255,213,127,0.40);"
                "}"
                "QPushButton:disabled { color: #7E6B55; border-color: rgba(76,48,31,0.25); }"
            );
            cardBtn->setCursor(Qt::PointingHandCursor);
            cardBtn->setEnabled(false);

            m_cardButtons.append(cardBtn);
            layout->addWidget(cardBtn);
        }

        layout->addStretch();

        // 开战按钮
        m_btnStartBattle = new QPushButton("开战", barContainer);
        m_btnStartBattle->setFixedSize(120, 50);
        m_btnStartBattle->setStyleSheet(
            "QPushButton {"
            "  background-color: rgba(225,176,99,0.92); color: #3A2418;"
            "  border: 2px solid rgba(76,48,31,0.82); border-radius: 10px;"
            "  font-size: 18px; font-weight: bold;"
            "}"
            "QPushButton:hover { border: 2px solid #FFD27E; }"
            "QPushButton:disabled { color: #7E6B55; border-color: rgba(76,48,31,0.25); }"
        );
        m_btnStartBattle->setCursor(Qt::PointingHandCursor);
        layout->addWidget(m_btnStartBattle);

        mainLayout->addWidget(barContainer);
    }

    // 页面背景
    this->setStyleSheet("DeployPage { background: transparent; }");

    // 设置卡牌按钮连接（只一次）
    setupCardButtonConnections();
}

void DeployPage::connectSignals()
{
    // 返回按钮
    connect(m_btnBack, &QPushButton::clicked, this, &DeployPage::signalBack);

    // 部署信号
    connect(m_deployView, &DeployView::signalDeployCard,
            this, [this](game::core::CardKind kind, game::core::MapPosition pos) {
        if (m_battleManager) {
            auto result = m_battleManager->deployCard(kind, pos);
            if (result) {
                m_deployedCount++;

                if (m_isPvp) {
                    sendDeployToNetwork(kind, pos);
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
        m_localReady = true;
        m_btnStartBattle->setEnabled(false);
        m_btnStartBattle->setText("等待对手...");

        if (m_isPvp) {
            sendDeploymentEnd();
            // Host 检查是否双方都准备好了
            if (m_isHost && m_opponentReady) {
                applyPendingOpponentDeploys();
                applyPendingOpponentOps();
                m_netCtx.server->sendPacket(game::network::MsgType::GAME_START);
                emit signalBattleStart();
            }
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
    m_localReady = false;
    m_opponentReady = false;
    m_pendingOpponentDeploys.clear();
    m_pendingOpponentOps.clear();

    // 设置地图
    setupMap();

    // 配置卡牌按钮（根据 deck 更新名称和费用）
    for (int i = 0; i < m_cardButtons.size(); ++i) {
        if (i < m_deck.size()) {
            m_cardButtons[i]->setEnabled(true);
            game::core::CardKind kind = m_deck[i];
            QString icon = cardIcon(kind);
            QString name = cardName(kind);
            int cost = game::core::CardSystem::deployCost(kind);
            m_cardButtons[i]->setText(QString("%1 %2\n💰%3").arg(icon).arg(name).arg(cost));
        } else {
            m_cardButtons[i]->setEnabled(false);
            m_cardButtons[i]->setText("空");
        }
    }

    // 连接网络信号（只连接一次，使用 UniqueConnection）
    if (m_isPvp) {
        if (m_isHost && m_netCtx.server) {
            connect(m_netCtx.server, &game::network::GameServer::packetReceived,
                    this, &DeployPage::onNetworkPacket, Qt::UniqueConnection);
        } else if (!m_isHost && m_netCtx.client) {
            connect(m_netCtx.client, &game::network::GameClient::packetReceived,
                    this, &DeployPage::onNetworkPacket, Qt::UniqueConnection);
        }
    }

    // 更新显示
    updateDeployCount();
    m_btnStartBattle->setEnabled(true);
    m_btnStartBattle->setText("开战");
    m_opponentLabel->setText("对手: 等待中...");

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
                    "QPushButton {"
                    "  background-color: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                    "    stop:0 rgba(225,176,99,0.95), stop:1 rgba(123,82,50,0.92));"
                    "  color: #3A2418;"
                    "  border: 2px solid rgba(76,48,31,0.82); border-radius: 8px;"
                    "  font-size: 11px; font-weight: bold;"
                    "}"
                );
            }
            m_cardButtons[i]->setStyleSheet(
                "QPushButton {"
                "  background-color: rgba(255,213,127,0.56);"
                "  color: #24150D;"
                "  border: 2px solid #FFD27E; border-radius: 8px;"
                "  font-size: 11px; font-weight: bold;"
                "}"
            );
        });
    }
}

void DeployPage::setupMap()
{
    auto& map = m_battleManager->map();

    // 新 PVP 地图布局（12x18）
    // 出怪口在上方 (1,6) 和 (1,11)
    // A方核心在左下角 (10,1)，B方核心在右下角 (10,16)

    // 1) 设置 NoDeploy 边框
    for (int r = 0; r < map.rows(); ++r) {
        map.setGrid({r, 0}, game::core::TerrainType::NoDeploy, 0);
        map.setGrid({r, map.cols() - 1}, game::core::TerrainType::NoDeploy, 0);
    }
    for (int c = 0; c < map.cols(); ++c) {
        map.setGrid({0, c}, game::core::TerrainType::NoDeploy, 0);
        map.setGrid({map.rows() - 1, c}, game::core::TerrainType::NoDeploy, 0);
    }

    // 2) 设置出生点
    game::core::MapPosition spawn1(1, 6);   // 上方出生点
    game::core::MapPosition spawn2(1, 11);  // 左上角出生点
    map.setGrid(spawn1, game::core::TerrainType::SpawnPoint, 0);
    map.setGrid(spawn2, game::core::TerrainType::SpawnPoint, 0);

    // 3) 设置核心
    game::core::MapPosition coreA(10, 1);   // A方核心（左下角）
    game::core::MapPosition coreB(10, 16);  // B方核心（右下角）
    map.setGrid(coreA, game::core::TerrainType::CoreA, 0);
    map.setGrid(coreB, game::core::TerrainType::CoreB, 0);

    // 4) 设置路径
    // 从出生点1向下到分叉点
    std::vector<game::core::MapPosition> path1_down = {
        {2,6}, {3,6}, {4,6}, {5,6}, {6,6}, {7,6}, {8,6}
    };
    // 从出生点2向下到分叉点
    std::vector<game::core::MapPosition> path2_down = {
        {2,11}, {3,11}, {4,11}, {5,11}, {6,11}, {7,11}, {8,11}
    };
    // 横向连接路径
    std::vector<game::core::MapPosition> path_horizontal = {
        {8,7}, {8,8}, {8,9}, {8,10}
    };
    // 向左下到A方核心
    std::vector<game::core::MapPosition> path_toA = {
        {9,6}, {9,5}, {9,4}, {9,3}, {9,2}, {9,1}
    };
    // 向右下到B方核心
    std::vector<game::core::MapPosition> path_toB = {
        {9,11}, {9,12}, {9,13}, {9,14}, {9,15}, {9,16}
    };

    // 标记所有路径
    for (const auto& pos : path1_down)
        map.setGrid(pos, game::core::TerrainType::Path, 0);
    for (const auto& pos : path2_down)
        map.setGrid(pos, game::core::TerrainType::Path, 0);
    for (const auto& pos : path_horizontal)
        map.setGrid(pos, game::core::TerrainType::Path, 0);
    for (const auto& pos : path_toA)
        map.setGrid(pos, game::core::TerrainType::Path, 0);
    for (const auto& pos : path_toB)
        map.setGrid(pos, game::core::TerrainType::Path, 0);

    // 5) 设置高台（战略要地）
    std::vector<game::core::MapPosition> highGround = {
        // A方区域的高台
        {3,2}, {3,3}, {5,2}, {5,3}, {7,2}, {7,3},
        // B方区域的高台
        {3,14}, {3,15}, {5,14}, {5,15}, {7,14}, {7,15},
        // 中间区域的高台
        {4,8}, {4,9}, {6,8}, {6,9}
    };
    for (const auto& pos : highGround)
        map.setGrid(pos, game::core::TerrainType::HighGround, 1);

    // 6) 设置双核心路线。WaveSpawner 会按怪物序号轮流分配，保证两边压力接近。
    std::vector<game::core::MapPosition> pathToA;
    pathToA.push_back(spawn1);
    for (const auto& pos : path1_down)
        pathToA.push_back(pos);
    for (const auto& pos : path_toA)
        pathToA.push_back(pos);
    pathToA.push_back(coreA);

    std::vector<game::core::MapPosition> pathToB;
    pathToB.push_back(spawn2);
    for (const auto& pos : path2_down)
        pathToB.push_back(pos);
    for (const auto& pos : path_toB)
        pathToB.push_back(pos);
    pathToB.push_back(coreB);

    m_battleManager->setPaths({pathToA, pathToB});

    // 根据玩家角色设置显示标记
    m_deployView->m_spawnPos = spawn1;
    m_deployView->m_corePos = m_isHost ? coreA : coreB;
}

void DeployPage::updateDeployCount()
{
    int resources = m_battleManager ? m_battleManager->resources().resources() : 0;
    m_deployCountLabel->setText(QString("已部署: %1 资源: %2").arg(m_deployedCount).arg(resources));
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
    m_localReady = false;
    m_opponentReady = false;
    m_btnStartBattle->setEnabled(true);
    m_btnStartBattle->setText("开战");
    m_opponentLabel->setText("对手: 等待中...");
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

void DeployPage::sendDeployToNetwork(game::core::CardKind kind, game::core::MapPosition pos)
{
    game::network::DeployPayload payload;
    payload.cardKind = static_cast<quint8>(kind);
    payload.row = static_cast<quint8>(pos.row);
    payload.col = static_cast<quint8>(pos.col);
    payload.unitId = 0;

    QByteArray body(reinterpret_cast<const char*>(&payload), sizeof(payload));

    if (m_isHost && m_netCtx.server) {
        m_netCtx.server->sendPacket(game::network::MsgType::DEPLOY, body);
    } else if (!m_isHost && m_netCtx.client) {
        m_netCtx.client->sendPacket(game::network::MsgType::DEPLOY, body);
    }
}

void DeployPage::sendUpgradeToNetwork(int unitId)
{
    game::network::UpgradePayload payload;
    payload.unitId = static_cast<quint8>(unitId);
    payload.targetLevel = 0;
    QByteArray body(reinterpret_cast<const char*>(&payload), sizeof(payload));

    if (m_isHost && m_netCtx.server) {
        m_netCtx.server->sendPacket(game::network::MsgType::UPGRADE_UNIT, body);
    } else if (!m_isHost && m_netCtx.client) {
        m_netCtx.client->sendPacket(game::network::MsgType::UPGRADE_UNIT, body);
    }
}

void DeployPage::sendMoveToNetwork(int unitId, game::core::MapPosition pos)
{
    QByteArray body;
    body.append(static_cast<char>(unitId));
    body.append(static_cast<char>(pos.row));
    body.append(static_cast<char>(pos.col));

    if (m_isHost && m_netCtx.server) {
        m_netCtx.server->sendPacket(game::network::MsgType::MOVE_UNIT, body);
    } else if (!m_isHost && m_netCtx.client) {
        m_netCtx.client->sendPacket(game::network::MsgType::MOVE_UNIT, body);
    }
}

void DeployPage::sendRecallToNetwork(int unitId)
{
    game::network::RecallPayload payload;
    payload.unitId = static_cast<quint8>(unitId);
    QByteArray body(reinterpret_cast<const char*>(&payload), sizeof(payload));

    if (m_isHost && m_netCtx.server) {
        m_netCtx.server->sendPacket(game::network::MsgType::RECALL_UNIT, body);
    } else if (!m_isHost && m_netCtx.client) {
        m_netCtx.client->sendPacket(game::network::MsgType::RECALL_UNIT, body);
    }
}

void DeployPage::applyPendingOpponentDeploys()
{
    if (!m_battleManager || m_pendingOpponentDeploys.isEmpty()) return;

    for (const auto& pending : m_pendingOpponentDeploys) {
        auto deployed = m_battleManager->deployOpponentCard(pending.kind, pending.position);
        if (!deployed) {
            qDebug() << "[DeployPage] failed to reveal opponent DEPLOY at"
                     << pending.position.row << pending.position.col;
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

void DeployPage::sendDeploymentEnd()
{
    if (m_isHost && m_netCtx.server) {
        m_netCtx.server->sendPacket(game::network::MsgType::DEPLOYMENT_END);
    } else if (!m_isHost && m_netCtx.client) {
        m_netCtx.client->sendPacket(game::network::MsgType::DEPLOYMENT_END);
    }

    m_opponentLabel->setText("对手: 已完成部署");
    m_opponentLabel->setStyleSheet("color: #00E676; font-size: 16px;");
}

void DeployPage::onNetworkPacket(game::network::MsgType type, const QByteArray& body)
{
    switch (type) {
    case game::network::MsgType::DEPLOY: {
        // 迷雾部署阶段只缓存对方本轮新部署，不立即写入 BattleManager。
        // 这样部署阶段只能看到上一轮战斗已经暴露过的单位；本轮新部署到开战时再揭示。
        if (body.size() >= 4) {
            auto kind = static_cast<game::core::CardKind>(static_cast<quint8>(body[0]));
            int row = static_cast<quint8>(body[1]);
            int col = static_cast<quint8>(body[2]);
            m_pendingOpponentDeploys.append({kind, game::core::MapPosition(row, col)});
            qDebug() << "[DeployPage] cached hidden opponent DEPLOY:" << (int)kind
                     << "at" << row << col;
        }
        break;
    }
    case game::network::MsgType::DEPLOYMENT_END: {
        m_opponentReady = true;
        m_opponentLabel->setText("对手: 已完成部署");
        m_opponentLabel->setStyleSheet("color: #00E676; font-size: 16px;");

        // Host 检查双方是否都准备好了
        if (m_isHost && m_localReady && m_opponentReady) {
            applyPendingOpponentDeploys();
            applyPendingOpponentOps();
            m_netCtx.server->sendPacket(game::network::MsgType::GAME_START);
            emit signalBattleStart();
        }
        break;
    }
    case game::network::MsgType::GAME_START: {
        // Client 收到开战信号
        if (!m_isHost) {
            applyPendingOpponentDeploys();
            applyPendingOpponentOps();
            emit signalBattleStart();
        }
        break;
    }
    case game::network::MsgType::UPGRADE_UNIT: {
        if (body.size() >= 1) {
            int unitId = static_cast<quint8>(body[0]);
            m_pendingOpponentOps.append({type, unitId, {}});
            qDebug() << "[DeployPage] cached hidden opponent UPGRADE:" << unitId;
        }
        break;
    }
    case game::network::MsgType::MOVE_UNIT: {
        if (body.size() >= 3) {
            int unitId = static_cast<quint8>(body[0]);
            int row = static_cast<quint8>(body[1]);
            int col = static_cast<quint8>(body[2]);
            m_pendingOpponentOps.append({type, unitId, game::core::MapPosition(row, col)});
            qDebug() << "[DeployPage] cached hidden opponent MOVE:" << unitId << row << col;
        }
        break;
    }
    case game::network::MsgType::RECALL_UNIT: {
        if (body.size() >= 1) {
            int unitId = static_cast<quint8>(body[0]);
            m_pendingOpponentOps.append({type, unitId, {}});
            qDebug() << "[DeployPage] cached hidden opponent RECALL:" << unitId;
        }
        break;
    }
    default:
        break;
    }
}
