#ifndef TECHBUTTON_H
#define TECHBUTTON_H

#include <QPushButton>
#include <QColor>

class TechButton : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(qreal hoverProgress READ hoverProgress WRITE setHoverProgress)

public:
    explicit TechButton(const QString &text = "", QWidget *parent = nullptr);

    void setAccentColor(const QColor &color);
    void setFontSize(int size);
    void setBorderRadius(qreal radius);

    qreal hoverProgress() const;
    void setHoverProgress(qreal progress);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    qreal m_hoverProgress = 0.0;
    QColor m_accentColor{0, 212, 255};   // 默认青色
    int m_fontSize = 16;
    qreal m_borderRadius = 14.0;
};

#endif // TECHBUTTON_H
