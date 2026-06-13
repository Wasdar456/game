#ifndef LEAFTRANSITIONOVERLAY_H
#define LEAFTRANSITIONOVERLAY_H

#include <QWidget>

#include <functional>

class QVariantAnimation;

class LeafTransitionOverlay final : public QWidget
{
public:
    explicit LeafTransitionOverlay(QWidget *parent = nullptr);
    void play(std::function<void()> midpointAction);
    bool isRunning() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVariantAnimation *m_animation;
    std::function<void()> m_midpointAction;
    qreal m_progress;
    bool m_midpointDone;
};

#endif // LEAFTRANSITIONOVERLAY_H
