#ifndef ARTHOTSPOT_H
#define ARTHOTSPOT_H

#include <QColor>
#include <QPixmap>
#include <QRectF>
#include <QVariantAnimation>
#include <QWidget>

#include <functional>

class ArtHotspot final : public QWidget
{
public:
    ArtHotspot(const QString &imagePath, const QRect &sourceRect,
               QWidget *parent = nullptr);

    void setCanvasRect(const QRectF &rect);
    void setClickHandler(std::function<void()> handler);
    void setSelected(bool selected);
    void setGlowColor(const QColor &color);
    void setSwayEnabled(bool enabled);
    void refreshVisual();

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void animateTo(qreal scale, qreal glow);

    QPixmap m_sourceImage;
    QRect m_sourceRect;
    QSizeF m_baseSize;
    QVariantAnimation *m_animation;
    std::function<void()> m_clickHandler;
    QColor m_glowColor;
    qreal m_scale;
    qreal m_glow;
    bool m_selected;
    bool m_pressed;
    bool m_swayEnabled;
    qreal m_idlePhase;
};

#endif // ARTHOTSPOT_H
