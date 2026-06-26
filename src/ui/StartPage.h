#ifndef STARTPAGE_H
#define STARTPAGE_H

#include <QRectF>
#include <QPixmap>
#include <QTimer>
#include <QVector>
#include <QWidget>

class ArtHotspot;
class QLabel;
class QKeyEvent;
class QMouseEvent;
class QResizeEvent;
class QShowEvent;

class StartPage : public QWidget
{
    Q_OBJECT

public:
    explicit StartPage(QWidget *parent = nullptr);
    void playEnterAnimation();

signals:
    void signalPveClicked();
    void signalPvpClicked();
    void signalAtlasClicked();
    void signalSettingsClicked();
    void signalExitClicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    bool m_introActive;
    bool m_splashActive;
    QWidget *m_menuLayer;
    QWidget *m_fadeOverlay;
    QLabel *m_pressHint;
    QRectF m_canvasRect;
    QPixmap m_menuCache;
    QSize m_menuCacheSize;
    QVector<ArtHotspot*> m_buttons;
    QTimer m_ambientTimer;
    qreal m_ambientPhase;
    qreal m_introElapsed;
    qreal m_menuRevealProgress;

    void initUI();
    void finishIntro();
    void revealMenu();
    void updateArtworkLayout();
    void updateFadeOverlay();
};

#endif // STARTPAGE_H
