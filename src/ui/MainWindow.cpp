/**
 * @file MainWindow.cpp
 * @brief 主窗口类实现文件
 *
 * 核心逻辑：
 *   1. 创建 BattleManager 实例（游戏核心逻辑管理器）
 *   2. initUI() —— 创建 QStackedWidget 和所有子页面，按顺序添加
 *   3. connectSignals() —— 将每个页面的导航信号连接到页面切换的槽函数
 *   4. 页面切换通过 setCurrentWidget() 实现
 *
 * 关于 BattleManager 的生命周期：
 *   - BattleManager 在 MainWindow 构造时创建，析构时销毁
 *   - 一局战斗结束后调用 clearBattle() 清空状态，而不是销毁重建
 *   - 新战斗开始时通过 BattleManager 的接口重新初始化
 */

#include "ui/MainWindow.h"
#include <QApplication>
#include <QPropertyAnimation>

// ========== 引入核心层头文件 ==========
// BattleManager 是 core 层对 UI 暴露的主要入口
// 它组合了 Map、ResourceManager、CardSystem、SkillSystem、WaveSpawner
#include "core/systems/BattleManager.h"

// ========== 引入各子页面的完整头文件 ==========
#include "ui/StartPage.h"
#include "ui/LobbyPage.h"
#include "ui/DeckPage.h"
#include "ui/BattlePage.h"
#include "ui/SettingsPage.h"

// ========== 构造函数 ==========
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_battleManager(nullptr)
    , m_stackWidget(nullptr)
    , m_startPage(nullptr)
    , m_lobbyPage(nullptr)
    , m_deckPage(nullptr)
    , m_battlePage(nullptr)
    , m_settingsPage(nullptr)
    , m_previousPage(nullptr)
{
    // ----- 创建 BattleManager -----
    // BattleManager 是游戏核心逻辑的总管理器
    // 它内部管理 Map、ResourceManager、CardSystem、SkillSystem、WaveSpawner
    // UI 层通过它的接口进行：部署卡牌、升级、移动、撤回、推进波次
    // 并通过 snapshot() 获取只读快照来渲染界面
    m_battleManager = new game::core::BattleManager();

    // 设置窗口基本属性
    this->setWindowTitle("塔防对战");
    this->setMinimumSize(1280, 720);
    this->resize(1280, 720);

    // 初始化 UI 和信号连接
    initUI();
    connectSignals();
}

// ========== 析构函数 ==========
MainWindow::~MainWindow()
{
    // BattleManager 不在 Qt 对象树中，需要手动释放
    delete m_battleManager;
}

// ========== initUI() —— 初始化所有 UI 组件 ==========
void MainWindow::initUI()
{
    // ----- 创建 QStackedWidget -----
    // QStackedWidget 在同一位置堆叠多个 Widget，但同一时刻只显示其中一个
    m_stackWidget = new QStackedWidget(this);

    // ----- 创建所有子页面 -----
    // 每个子页面都需要获取 BattleManager 的访问权限
    // 通过传入 this（MainWindow 指针），子页面可调用 battleManager() 获取核心层接口

    m_startPage    = new StartPage(this);
    m_lobbyPage    = new LobbyPage(this);
    m_deckPage     = new DeckPage(this);
    m_battlePage   = new BattlePage(this);
    m_settingsPage = new SettingsPage(this);

    // ----- 将所有页面按顺序添加到堆叠窗口 -----
    m_stackWidget->addWidget(m_startPage);      // index 0
    m_stackWidget->addWidget(m_lobbyPage);      // index 1
    m_stackWidget->addWidget(m_deckPage);       // index 2
    m_stackWidget->addWidget(m_battlePage);     // index 3
    m_stackWidget->addWidget(m_settingsPage);   // index 4

    // ----- 设置堆叠窗口为主窗口的中央控件 -----
    this->setCentralWidget(m_stackWidget);

    // ----- 默认显示起始页 -----
    m_stackWidget->setCurrentWidget(m_startPage);
}

// ========== 带淡入淡出效果的页面切换 ==========
void MainWindow::fadeToPage(QWidget *page)
{
    QWidget *current = m_stackWidget->currentWidget();
    if (current == page) return;

    // 淡出当前页
    QGraphicsOpacityEffect *outEffect = new QGraphicsOpacityEffect(current);
    current->setGraphicsEffect(outEffect);
    QPropertyAnimation *outAnim = new QPropertyAnimation(outEffect, "opacity");
    outAnim->setDuration(150);
    outAnim->setStartValue(1.0);
    outAnim->setEndValue(0.0);
    outAnim->setEasingCurve(QEasingCurve::OutCubic);

    // 淡入目标页
    QGraphicsOpacityEffect *inEffect = new QGraphicsOpacityEffect(page);
    page->setGraphicsEffect(inEffect);
    QPropertyAnimation *inAnim = new QPropertyAnimation(inEffect, "opacity");
    inAnim->setDuration(200);
    inAnim->setStartValue(0.0);
    inAnim->setEndValue(1.0);
    inAnim->setEasingCurve(QEasingCurve::InCubic);

    // 淡出完成后切换页面并开始淡入
    connect(outAnim, &QPropertyAnimation::finished, this, [this, page, inAnim]() {
        m_stackWidget->setCurrentWidget(page);
        inAnim->start(QAbstractAnimation::DeleteWhenStopped);
    });

    outAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

// ========== connectSignals() —— 连接信号与槽 ==========
void MainWindow::connectSignals()
{
    // ===== 起始页 (StartPage) 的信号连接 =====

    // 点击 [单人PVE] → 切换到大厅页，并设置模式为 PVE
    connect(m_startPage, &StartPage::signalPveClicked,
            this, [this]() {
                m_lobbyPage->setMode(LobbyPage::Mode::PVE);
                fadeToPage(m_lobbyPage);
            });

    // 点击 [多人PVP] → 切换到大厅页，并设置模式为 PVP
    connect(m_startPage, &StartPage::signalPvpClicked,
            this, [this]() {
                m_lobbyPage->setMode(LobbyPage::Mode::PVP);
                fadeToPage(m_lobbyPage);
            });

    // 点击 [图鉴/仓库] → 切换到选卡页（记录来源：起始页）
    connect(m_startPage, &StartPage::signalAtlasClicked,
            this, [this]() {
                m_previousPage = m_startPage;
                fadeToPage(m_deckPage);
            });

    // 点击 [设置] → 切换到设置页
    connect(m_startPage, &StartPage::signalSettingsClicked,
            this, [this]() {
                fadeToPage(m_settingsPage);
            });

    // 点击 [退出] → 关闭整个应用程序
    connect(m_startPage, &StartPage::signalExitClicked,
            this, &QApplication::quit);

    // ===== 大厅页 (LobbyPage) 的信号连接 =====

    // 大厅配置完成 → 切换到选卡页（记录来源：大厅页）
    connect(m_lobbyPage, &LobbyPage::signalConfigDone,
            this, [this]() {
                m_previousPage = m_lobbyPage;
                fadeToPage(m_deckPage);
            });

    // 点击 [返回] → 回到起始页
    connect(m_lobbyPage, &LobbyPage::signalBack,
            this, [this]() {
                fadeToPage(m_startPage);
            });

    // ===== 选卡页 (DeckPage) 的信号连接 =====

    // 卡组编满并点击 [开始战斗] → 切换到战斗页
    connect(m_deckPage, &DeckPage::signalBattleStart,
            this, [this]() {
                m_battlePage->startBattle();
                fadeToPage(m_battlePage);
            });

    // 点击 [返回] → 回到上一页
    connect(m_deckPage, &DeckPage::signalBack,
            this, [this]() {
                if (m_previousPage) {
                    fadeToPage(m_previousPage);
                } else {
                    fadeToPage(m_startPage);
                }
            });

    // ===== 战斗页 (BattlePage) 的信号连接 =====

    // 战斗结束 → 回到起始页
    connect(m_battlePage, &BattlePage::signalBattleEnd,
            this, [this]() {
                m_battleManager->clearBattle();
                fadeToPage(m_startPage);
            });

    // ===== 设置页 (SettingsPage) 的信号连接 =====

    // 点击 [返回] → 回到起始页
    connect(m_settingsPage, &SettingsPage::signalBack,
            this, [this]() {
                fadeToPage(m_startPage);
            });
}
