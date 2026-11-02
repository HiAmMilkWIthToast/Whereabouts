#include "filterchip.h"
#include "theme.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

FilterChip::FilterChip(const QString &label, int count, QWidget *parent)
    : QPushButton(parent)
    , m_label(label)
    , m_count(count)
    , m_countColor(Theme::accent())
{
    setObjectName(QStringLiteral("filterChip"));
    setCursor(Qt::PointingHandCursor);
    setCheckable(true);
    setChecked(true);
    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void FilterChip::layoutText() const
{
    const QFontMetrics metrics(font());
    m_labelWidth = metrics.horizontalAdvance(m_label);
    m_countWidth = metrics.horizontalAdvance(QString::number(m_count));
}

int FilterChip::leftInset() const
{
    if (m_labelWidth < 0)
        layoutText();
    const int content = m_labelWidth + kGap + m_countWidth;
    return (width() - content) / 2;
}

int FilterChip::rightInset() const
{
    if (m_labelWidth < 0)
        layoutText();
    const int content = m_labelWidth + kGap + m_countWidth;
    return width() - content - leftInset();
}

QSize FilterChip::sizeHint() const
{
    if (m_labelWidth < 0)
        layoutText();
    const QFontMetrics metrics(font());
    // Content plus two equal insets. No fudge term is added: an extra pixel
    // here would land entirely on one side and make the padding asymmetric.
    const int w = kPadX * 2 + m_labelWidth + kGap + m_countWidth;
    const int h = metrics.height() + 5;
    return QSize(w, h);
}

void FilterChip::enterEvent(QEnterEvent *event)
{
    // paintEvent brightens the pill when underMouse() is true, so the widget has
    // to be repainted when that answer changes.
    update();
    QPushButton::enterEvent(event);
}

void FilterChip::leaveEvent(QEvent *event)
{
    update();
    QPushButton::leaveEvent(event);
}

void FilterChip::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // Fully rounded ends: half the height, so the pill never looks like a
    // rectangle with clipped corners at any chip height.
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = r.height() / 2.0;

    const bool on = isChecked();
    const bool hot = underMouse();

    QColor fill = on ? QColor(Theme::accentDimName()) : QColor(QStringLiteral("#14171b"));
    QColor border = on ? Theme::accent() : QColor(QStringLiteral("#24292f"));
    QColor textColor = on ? Theme::accent() : QColor(QStringLiteral("#828a94"));

    if (hot) {
        if (!on) {
            border = QColor(QStringLiteral("#333a43"));
            textColor = QColor(QStringLiteral("#c9ccd2"));
        }
        fill = fill.lighter(108);
    }

    painter.setPen(QPen(border, 1.0));
    painter.setBrush(fill);
    painter.drawRoundedRect(r, radius, radius);

    if (m_labelWidth < 0)
        layoutText();

    // The label and the count are drawn from one measured run, so the whole
    // content is centred as a unit instead of each part drifting apart.
    const int content = m_labelWidth + kGap + m_countWidth;
    const int x = (width() - content) / 2;
    const QRectF area(x, 0, content, height());

    QFont labelFont = font();
    painter.setFont(labelFont);
    painter.setPen(textColor);
    painter.drawText(QRectF(area.left(),
                            area.top(),
                            m_labelWidth,
                            area.height()),
                     Qt::AlignLeft | Qt::AlignVCenter, m_label);

    // The count is dimmer while switched on, and muted further when off, so it
    // never competes with the label it belongs to.
    QColor countColor = m_countColor;
    if (!on)
        countColor = QColor(QStringLiteral("#5f666e"));
    else if (hot)
        countColor = countColor.lighter(112);

    painter.setPen(countColor);
    painter.drawText(QRectF(area.left() + m_labelWidth + kGap,
                            area.top(),
                            m_countWidth,
                            area.height()),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QString::number(m_count));
}
