#ifndef LEAFTRANSITIONOVERLAY_H
#define LEAFTRANSITIONOVERLAY_H

#include <QPixmap>
#include <QWidget>

#include <functional>

class QVariantAnimation;

class LeafTransitionOverlay final : public QWidget
{
public:
    explicit LeafTransitionOverlay(QWidget *parent = nullptr);
    void play(std::function<void()> revealAction,
              std::function<void()> finishedAction = {});
    bool isRunning() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVariantAnimation *m_animation;
    QPixmap m_smoke;
    QPixmap m_scaledSmoke;
    QSize m_smokeViewport;
    std::function<void()> m_finishedAction;
    qreal m_progress;
};

#endif // LEAFTRANSITIONOVERLAY_H
