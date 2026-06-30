#ifndef TUTORIALOVERLAY_H
#define TUTORIALOVERLAY_H

#include <QPixmap>
#include <QRect>
#include <QTimer>
#include <QWidget>

class QLabel;
class QPushButton;

class TutorialOverlay final : public QWidget
{
    Q_OBJECT

public:
    enum class Focus {
        None,
        Resource,
        Cards,
        Battlefield,
        Core
    };

    explicit TutorialOverlay(QWidget *parent = nullptr);

    void setTargets(const QRect& resource,
                    const QRect& cards,
                    const QRect& battlefield,
                    const QRect& core);
    void showStep(const QString& speaker,
                  const QString& text,
                  Focus focus,
                  const QString& actionText);
    void closeOverlay();

signals:
    void signalAction();
    void signalSkip();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QRect focusRect() const;
    void updateLayout();

    QLabel *m_speakerLabel;
    QLabel *m_textLabel;
    QPushButton *m_actionButton;
    QPushButton *m_skipButton;
    QRect m_resourceTarget;
    QRect m_cardsTarget;
    QRect m_battlefieldTarget;
    QRect m_coreTarget;
    QRectF m_panelRect;
    Focus m_focus;
    QPixmap m_portraitAtlas;
    QTimer m_pulseTimer;
    qreal m_phase;
};

#endif // TUTORIALOVERLAY_H
