#ifndef DECKPAGE_H
#define DECKPAGE_H

#include "ui/CardCollection.h"

#include <QColor>
#include <QLabel>
#include <QPushButton>
#include <QRectF>
#include <QScrollArea>
#include <QVector>
#include <QWidget>

class ArtHotspot;
class QResizeEvent;
class QShowEvent;

struct CardDisplayInfo {
    game::core::CardKind kind;
    QString name;
    int deployCost;
    int maxHp;
    int attack;
    int attackRange;
    int moveLimit;
    double attackInterval;
    QString skillDesc;
    QString priorityDesc;
    QColor themeColor;
};

class DeckPage : public QWidget
{
    Q_OBJECT

public:
    static const int MAX_DECK_SLOTS = 5;

    explicit DeckPage(QWidget *parent = nullptr);
    QVector<game::core::CardKind> getSelectedKinds() const;

signals:
    void signalBattleStart();
    void signalBack();
    void signalDeckSelected(const QVector<game::core::CardKind> &selectedKinds);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QPushButton *m_btnBack;
    QLabel      *m_titleLabel;
    QLabel      *m_detailPanel;
    QPushButton *m_btnStartBattle;
    QLabel      *m_ticketLabel;
    QPushButton *m_btnDrawPanel;
    QWidget     *m_drawOverlay;
    QLabel      *m_drawTitleLabel;
    QLabel      *m_drawBodyLabel;
    QLabel      *m_drawResultLabel;
    QWidget     *m_drawCardsPanel;
    QPushButton *m_btnDrawOne;
    QPushButton *m_btnDrawTen;
    QPushButton *m_btnCloseDraw;
    QPushButton *m_btnUpgradeCard;

    QVector<CardDisplayInfo> m_allCards;
    QVector<int> m_selectedSlots;
    int m_selectedCardIndex;

    QVector<QPushButton*> m_slotButtons;
    QScrollArea *m_cardPoolScroll;
    QRectF m_canvasRect;
    QVector<ArtHotspot*> m_cardHotspots;
    QVector<QLabel*> m_cardLockLabels;
    ArtHotspot *m_backHotspot;
    ArtHotspot *m_startHotspot;
    QVector<QWidget*> m_drawCardFrames;
    QVector<QLabel*> m_drawCardArtLabels;
    QVector<QLabel*> m_drawCardNameLabels;
    QVector<QLabel*> m_drawCardBadgeLabels;
    int m_lastDrawResultCount;
    bool m_drawOverlayVisible;

    void initUI();
    void createCardPoolData();
    void connectSignals();
    void updateArtworkLayout();
    void refreshDeckSlotsDisplay();
    void refreshDetailPanel(int cardIndex);
    void updateStartBattleButton();
    void animateCardToSlot(int cardIndex, int slotIndex);
    void refreshCollectionDisplay();
    void openDrawPanel();
    void performDraw(int count);
    QString cardNameForKind(game::core::CardKind kind) const;
    int indexForKind(game::core::CardKind kind) const;
    void ensureDrawCardWidgets(int count);
    void layoutDrawCards();
    void showDrawInstructions();
    void clearDrawCards();
    void populateDrawResults(const QVector<DrawResult>& results);
    void refreshDrawOverlayLayout();
    void setDrawOverlayVisible(bool visible);
    void updateLockLabelVisibilityForOverlay();
};

#endif // DECKPAGE_H
