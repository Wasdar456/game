/**
 * @file MainWindow.cpp
 * @brief 涓荤獥鍙ｇ被瀹炵幇鏂囦欢
 *
 * 鏍稿績閫昏緫锛?
 *   1. 鍒涘缓 BattleManager 瀹炰緥锛堟父鎴忔牳蹇冮€昏緫绠＄悊鍣級
 *   2. initUI() 鈥斺€?鍒涘缓 QStackedWidget 鍜屾墍鏈夊瓙椤甸潰锛屾寜椤哄簭娣诲姞
 *   3. connectSignals() 鈥斺€?灏嗘瘡涓〉闈㈢殑瀵艰埅淇″彿杩炴帴鍒伴〉闈㈠垏鎹㈢殑妲藉嚱鏁?
 *   4. 椤甸潰鍒囨崲閫氳繃 setCurrentWidget() 瀹炵幇
 *
 * 鍏充簬 BattleManager 鐨勭敓鍛藉懆鏈燂細
 *   - BattleManager 鍦?MainWindow 鏋勯€犳椂鍒涘缓锛屾瀽鏋勬椂閿€姣?
 *   - 涓€灞€鎴樻枟缁撴潫鍚庤皟鐢?clearBattle() 娓呯┖鐘舵€侊紝鑰屼笉鏄攢姣侀噸寤?
 *   - 鏂版垬鏂楀紑濮嬫椂閫氳繃 BattleManager 鐨勬帴鍙ｉ噸鏂板垵濮嬪寲
 */

#include "ui/MainWindow.h"
#include <QApplication>
#include <QPropertyAnimation>
#include <QTimer>

// ========== 寮曞叆鏍稿績灞傚ご鏂囦欢 ==========
// BattleManager 鏄?core 灞傚 UI 鏆撮湶鐨勪富瑕佸叆鍙?
// 瀹冪粍鍚堜簡 Map銆丷esourceManager銆丆ardSystem銆丼killSystem銆乄aveSpawner
#include "core/systems/BattleManager.h"

// ========== 寮曞叆鍚勫瓙椤甸潰鐨勫畬鏁村ご鏂囦欢 ==========
#include "ui/StartPage.h"
#include "ui/LobbyPage.h"
#include "ui/DeckPage.h"
#include "ui/DeployPage.h"
#include "ui/BattlePage.h"
#include "ui/SettingsPage.h"

// ========== 鏋勯€犲嚱鏁?==========
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_battleManager(nullptr)
    , m_stackWidget(nullptr)
    , m_startPage(nullptr)
    , m_lobbyPage(nullptr)
    , m_deckPage(nullptr)
    , m_deployPage(nullptr)
    , m_battlePage(nullptr)
    , m_settingsPage(nullptr)
    , m_previousPage(nullptr)
{
    // ----- 鍒涘缓 BattleManager -----
    // BattleManager 鏄父鎴忔牳蹇冮€昏緫鐨勬€荤鐞嗗櫒
    // 瀹冨唴閮ㄧ鐞?Map銆丷esourceManager銆丆ardSystem銆丼killSystem銆乄aveSpawner
    // UI 灞傞€氳繃瀹冪殑鎺ュ彛杩涜锛氶儴缃插崱鐗屻€佸崌绾с€佺Щ鍔ㄣ€佹挙鍥炪€佹帹杩涙尝娆?
    // 骞堕€氳繃 snapshot() 鑾峰彇鍙蹇収鏉ユ覆鏌撶晫闈?
    m_battleManager = new game::core::BattleManager();

    // 璁剧疆绐楀彛鍩烘湰灞炴€?
    this->setWindowTitle("濉旈槻瀵规垬");
    this->setMinimumSize(1280, 720);
    this->resize(1280, 720);

    // 鍒濆鍖?UI 鍜屼俊鍙疯繛鎺?
    initUI();
    connectSignals();
}

// ========== 鏋愭瀯鍑芥暟 ==========
MainWindow::~MainWindow()
{
    // BattleManager 涓嶅湪 Qt 瀵硅薄鏍戜腑锛岄渶瑕佹墜鍔ㄩ噴鏀?
    delete m_battleManager;
}

// ========== initUI() 鈥斺€?鍒濆鍖栨墍鏈?UI 缁勪欢 ==========
void MainWindow::initUI()
{
    // ----- 鍒涘缓 QStackedWidget -----
    // QStackedWidget 鍦ㄥ悓涓€浣嶇疆鍫嗗彔澶氫釜 Widget锛屼絾鍚屼竴鏃跺埢鍙樉绀哄叾涓竴涓?
    m_stackWidget = new QStackedWidget(this);

    // ----- 鍒涘缓鎵€鏈夊瓙椤甸潰 -----
    // 姣忎釜瀛愰〉闈㈤兘闇€瑕佽幏鍙?BattleManager 鐨勮闂潈闄?
    // 閫氳繃浼犲叆 this锛圡ainWindow 鎸囬拡锛夛紝瀛愰〉闈㈠彲璋冪敤 battleManager() 鑾峰彇鏍稿績灞傛帴鍙?

    m_startPage    = new StartPage(this);
    m_lobbyPage    = new LobbyPage(this);
    m_deckPage     = new DeckPage(this);
    m_deployPage   = new DeployPage(this);
    m_battlePage   = new BattlePage(this);
    m_settingsPage = new SettingsPage(this);

    // ----- 灏嗘墍鏈夐〉闈㈡寜椤哄簭娣诲姞鍒板爢鍙犵獥鍙?-----
    m_stackWidget->addWidget(m_startPage);      // index 0
    m_stackWidget->addWidget(m_lobbyPage);      // index 1
    m_stackWidget->addWidget(m_deckPage);       // index 2
    m_stackWidget->addWidget(m_deployPage);     // index 3
    m_stackWidget->addWidget(m_battlePage);     // index 4
    m_stackWidget->addWidget(m_settingsPage);   // index 5

    // ----- 璁剧疆鍫嗗彔绐楀彛涓轰富绐楀彛鐨勪腑澶帶浠?-----
    this->setCentralWidget(m_stackWidget);

    // ----- 榛樿鏄剧ず璧峰椤?-----
    m_stackWidget->setCurrentWidget(m_startPage);
}

// ========== 甯︽贰鍏ユ贰鍑烘晥鏋滅殑椤甸潰鍒囨崲 ==========
void MainWindow::fadeToPage(QWidget *page)
{
    QWidget *current = m_stackWidget->currentWidget();
    if (!page || current == page) {
        if (page) {
            page->setGraphicsEffect(nullptr);
            page->update();
        }
        return;
    }

    current->setGraphicsEffect(nullptr);
    page->setGraphicsEffect(nullptr);

    // 娣″嚭褰撳墠椤?
    QGraphicsOpacityEffect *outEffect = new QGraphicsOpacityEffect(current);
    current->setGraphicsEffect(outEffect);
    QPropertyAnimation *outAnim = new QPropertyAnimation(outEffect, "opacity");
    outAnim->setDuration(150);
    outAnim->setStartValue(1.0);
    outAnim->setEndValue(0.0);
    outAnim->setEasingCurve(QEasingCurve::OutCubic);

    // 娣″嚭瀹屾垚鍚庡垏鎹㈤〉闈㈠苟寮€濮嬫贰鍏?
    connect(outAnim, &QPropertyAnimation::finished, this, [this, current, page]() {
        QTimer::singleShot(0, current, [current]() {
            current->setGraphicsEffect(nullptr);
            current->update();
        });
        m_stackWidget->setCurrentWidget(page);
        page->show();
        page->raise();
        page->update();

        // showEvent() may refresh page visuals, so attach the fade-in effect afterwards.
        QGraphicsOpacityEffect *inEffect = new QGraphicsOpacityEffect(page);
        inEffect->setOpacity(0.0);
        page->setGraphicsEffect(inEffect);
        QPropertyAnimation *inAnim = new QPropertyAnimation(inEffect, "opacity");
        inAnim->setDuration(200);
        inAnim->setStartValue(0.0);
        inAnim->setEndValue(1.0);
        inAnim->setEasingCurve(QEasingCurve::InCubic);

        connect(inAnim, &QPropertyAnimation::finished, page, [page]() {
            QTimer::singleShot(0, page, [page]() {
                page->setGraphicsEffect(nullptr);
                page->update();
                page->repaint();
            });
        });
        inAnim->start(QAbstractAnimation::DeleteWhenStopped);
    });

    outAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

// ========== connectSignals() 鈥斺€?杩炴帴淇″彿涓庢Ы ==========
void MainWindow::connectSignals()
{
    // ===== 璧峰椤?(StartPage) 鐨勪俊鍙疯繛鎺?=====

    // 鐐瑰嚮 [鍗曚汉PVE] 鈫?鍒囨崲鍒板ぇ鍘呴〉锛屽苟璁剧疆妯″紡涓?PVE
    connect(m_startPage, &StartPage::signalPveClicked,
            this, [this]() {
                m_lobbyPage->setMode(LobbyPage::Mode::PVE);
                fadeToPage(m_lobbyPage);
            });

    // 鐐瑰嚮 [澶氫汉PVP] 鈫?鍒囨崲鍒板ぇ鍘呴〉锛屽苟璁剧疆妯″紡涓?PVP
    connect(m_startPage, &StartPage::signalPvpClicked,
            this, [this]() {
                m_lobbyPage->setMode(LobbyPage::Mode::PVP);
                fadeToPage(m_lobbyPage);
            });

    // 鐐瑰嚮 [鍥鹃壌/浠撳簱] 鈫?鍒囨崲鍒伴€夊崱椤碉紙璁板綍鏉ユ簮锛氳捣濮嬮〉锛?
    connect(m_startPage, &StartPage::signalAtlasClicked,
            this, [this]() {
                m_previousPage = m_startPage;
                fadeToPage(m_deckPage);
            });

    // 鐐瑰嚮 [璁剧疆] 鈫?鍒囨崲鍒拌缃〉
    connect(m_startPage, &StartPage::signalSettingsClicked,
            this, [this]() {
                fadeToPage(m_settingsPage);
            });

    // 鐐瑰嚮 [閫€鍑篯 鈫?鍏抽棴鏁翠釜搴旂敤绋嬪簭
    connect(m_startPage, &StartPage::signalExitClicked,
            this, &QApplication::quit);

    // ===== 澶у巺椤?(LobbyPage) 鐨勪俊鍙疯繛鎺?=====

    // PVE 澶у巺閰嶇疆瀹屾垚 鈫?鍒囨崲鍒伴€夊崱椤碉紙璁板綍鏉ユ簮锛氬ぇ鍘呴〉锛?
    connect(m_lobbyPage, &LobbyPage::signalConfigDone,
            this, [this](const QString& mapId) {
                m_networkContext = NetworkContext();  // 閲嶇疆涓洪粯璁わ紙闈濸VP锛?
                m_networkContext.pveMapId = mapId;
                m_previousPage = m_lobbyPage;
                fadeToPage(m_deckPage);
            });

    // PVP 澶у巺閰嶇疆瀹屾垚 鈫?淇濆瓨缃戠粶涓婁笅鏂囷紝鍒囨崲鍒伴€夊崱椤?
    connect(m_lobbyPage, &LobbyPage::signalPvpReady,
            this, [this](const NetworkContext& ctx) {
                m_networkContext = ctx;
                m_previousPage = m_lobbyPage;
                fadeToPage(m_deckPage);
            });

    // 鐐瑰嚮 [杩斿洖] 鈫?鍥炲埌璧峰椤?
    connect(m_lobbyPage, &LobbyPage::signalBack,
            this, [this]() {
                fadeToPage(m_startPage);
            });

    // ===== 閫夊崱椤?(DeckPage) 鐨勪俊鍙疯繛鎺?=====

    // 鍗＄粍缂栨弧骞剁偣鍑?[寮€濮嬫垬鏂梋
    connect(m_deckPage, &DeckPage::signalBattleStart,
            this, [this]() {
                if (m_networkContext.isPvp) {
                    // PVP 妯″紡锛氳繘鍏ヨ糠闆鹃儴缃查〉闈?
                    m_deployPage->setNetworkContext(m_networkContext);
                    m_deployPage->setDeck(m_deckPage->getSelectedKinds());
                    m_deployPage->initDeployment();
                    fadeToPage(m_deployPage);
                } else {
                    // PVE 妯″紡锛氱洿鎺ヨ繘鍏ユ垬鏂?
                    m_battlePage->setNetworkContext(m_networkContext);
                    m_battlePage->startBattle();
                    fadeToPage(m_battlePage);
                }
            });

    // 閫夊崱椤佃繑鍥?
    connect(m_deckPage, &DeckPage::signalBack,
            this, [this]() {
                if (m_previousPage) {
                    fadeToPage(m_previousPage);
                } else {
                    fadeToPage(m_startPage);
                }
            });

    // ===== 閮ㄧ讲椤?(DeployPage) 鐨勪俊鍙疯繛鎺?=====

    // 寮€鎴?鈫?鍒囨崲鍒版垬鏂楅〉
    connect(m_deployPage, &DeployPage::signalBattleStart,
            this, [this]() {
                m_battlePage->setNetworkContext(m_networkContext);
                m_battlePage->startBattle();
                fadeToPage(m_battlePage);
            });

    // 杩斿洖 鈫?鍥炲埌閫夊崱椤?
    connect(m_deployPage, &DeployPage::signalBack,
            this, [this]() {
                fadeToPage(m_deckPage);
            });

    // ===== 鎴樻枟椤?(BattlePage) 鐨勪俊鍙疯繛鎺?=====

    // 鎬墿娓呯┖ 鈫?鍥炲埌閮ㄧ讲闃舵
    connect(m_battlePage, &BattlePage::signalBackToDeploy,
            this, [this]() {
                m_deployPage->reEnter();
                fadeToPage(m_deployPage);
            });

    // 鎴樻枟缁撴潫 鈫?鍥炲埌璧峰椤?
    connect(m_battlePage, &BattlePage::signalBattleEnd,
            this, [this]() {
                m_battleManager->clearBattle();
                fadeToPage(m_startPage);
            });

    // ===== 璁剧疆椤?(SettingsPage) 鐨勪俊鍙疯繛鎺?=====

    // 鐐瑰嚮 [杩斿洖] 鈫?鍥炲埌璧峰椤?
    connect(m_settingsPage, &SettingsPage::signalBack,
            this, [this]() {
                fadeToPage(m_startPage);
            });
}
