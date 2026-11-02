#include "elidedlabel.h"

#include <QPainter>
#include <QStyle>
#include <QStyleOption>

ElidedLabel::ElidedLabel(QWidget *parent)
    : QLabel(parent)
{
    // Preferred, not Ignored: Ignored throws away the label's size hint, so the
    // header collapsed it to its minimum and widening the window gave it no more
    // room than narrowing it did. Preferred lets the layout hand it everything
    // on offer, and the elision in paintEvent copes when that is too little.
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    policy.setHeightForWidth(false);
    setSizePolicy(policy);
    setWordWrap(false);
    // The shortened text is produced at paint time, so the label must not also
    // draw whatever QLabel thinks it should show.
    setTextFormat(Qt::PlainText);
}

void ElidedLabel::setFullText(const QString &text)
{
    if (m_full == text)
        return;
    m_full = text;
    setToolTip(text);
    // QLabel's own text is kept empty; paintEvent supplies the drawing. This
    // keeps the two from disagreeing about what the label says.
    QLabel::setText(QString());
    updateGeometry();
    update();
}

QSize ElidedLabel::sizeHint() const
{
    // QLabel would report the hint of its (empty) text, which is a few pixels
    // and leaves the layout with no idea how much room the real text wants.
    const QMargins margins = contentsMargins();
    const int width = fontMetrics().horizontalAdvance(m_full)
        + margins.left() + margins.right();
    return QSize(width, QLabel::sizeHint().height());
}

QSize ElidedLabel::minimumSizeHint() const
{
    QSize hint = QLabel::minimumSizeHint();
    // Small enough that the header is not forced wider than the window, but not
    // zero: a label with no floor at all collapses to nothing before the layout
    // considers giving it room.
    hint.setWidth(fontMetrics().horizontalAdvance(QStringLiteral("...")));
    return hint;
}

void ElidedLabel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QStyleOption opt;
    opt.initFrom(this);

    QPainter painter(this);
    // The stylesheet's colour and font, applied the way QLabel would apply them.
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);

    const QRect area = rect();
    const QString shown = fontMetrics().elidedText(m_full, Qt::ElideMiddle, area.width());
    painter.setPen(palette().color(foregroundRole()));
    painter.setFont(font());
    // Aligned the way the stylesheet expects, with no vertical offset: the row
    // already sits in a layout that centres it.
    painter.drawText(area, Qt::AlignLeft | Qt::AlignVCenter, shown);
}
