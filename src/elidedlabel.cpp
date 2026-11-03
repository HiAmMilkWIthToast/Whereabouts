#include "elidedlabel.h"

#include <QPainter>
#include <QStyle>
#include <QStyleOption>

ElidedLabel::ElidedLabel(QWidget *parent)
    : QLabel(parent)
{

    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    policy.setHeightForWidth(false);
    setSizePolicy(policy);
    setWordWrap(false);

    setTextFormat(Qt::PlainText);
}

void ElidedLabel::setFullText(const QString &text)
{
    if (m_full == text)
        return;
    m_full = text;
    setToolTip(text);

    QLabel::setText(QString());
    updateGeometry();
    update();
}

QSize ElidedLabel::sizeHint() const
{

    const QMargins margins = contentsMargins();
    const int width = fontMetrics().horizontalAdvance(m_full)
        + margins.left() + margins.right();
    return QSize(width, QLabel::sizeHint().height());
}

QSize ElidedLabel::minimumSizeHint() const
{
    QSize hint = QLabel::minimumSizeHint();

    hint.setWidth(fontMetrics().horizontalAdvance(QStringLiteral("...")));
    return hint;
}

void ElidedLabel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QStyleOption opt;
    opt.initFrom(this);

    QPainter painter(this);

    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);

    const QRect area = rect();
    const QString shown = fontMetrics().elidedText(m_full, Qt::ElideMiddle, area.width());
    painter.setPen(palette().color(foregroundRole()));
    painter.setFont(font());

    painter.drawText(area, Qt::AlignLeft | Qt::AlignVCenter, shown);
}
