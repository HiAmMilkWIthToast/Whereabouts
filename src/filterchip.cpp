#include "filterchip.h"
#include "theme.h"

#include <QFontMetrics>
#include <QImage>
#include <QPainter>

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
    const QString countText = QString::number(m_count);
    m_labelAdvance = metrics.horizontalAdvance(m_label);
    m_countAdvance = metrics.horizontalAdvance(countText);
    m_countOriginX = m_labelAdvance + kGap;

    const int probeW = m_countOriginX + m_countAdvance + 8;
    const int probeH = metrics.height() + 4;

    int left = probeW;
    int right = -1;
    if (m_labelAdvance > 0 || m_countAdvance > 0) {
        QImage probe(probeW, probeH, QImage::Format_ARGB32_Premultiplied);
        probe.fill(Qt::transparent);
        QPainter p(&probe);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        p.setFont(font());
        p.setPen(Qt::black);
        p.drawText(QRectF(0, 0, probeW, probeH),
                   Qt::AlignLeft | Qt::AlignVCenter, m_label);
        p.drawText(QRectF(m_countOriginX, 0, probeW - m_countOriginX, probeH),
                   Qt::AlignLeft | Qt::AlignVCenter, countText);
        p.end();

        for (int y = 0; y < probeH; ++y) {
            for (int x = 0; x < probeW; ++x) {
                if (qAlpha(probe.pixel(x, y)) != 0) {
                    left = qMin(left, x);
                    right = qMax(right, x);
                }
            }
        }
    }

    if (right < left) {

        m_runLeft = 0;
        m_runWidth = m_labelAdvance + kGap + m_countAdvance;
        return;
    }

    m_runLeft = left;
    m_runWidth = right - left + 1;
}

int FilterChip::contentWidth() const
{
    return m_runWidth;
}

int FilterChip::leftInset() const
{
    layoutText();
    return (width() - contentWidth()) / 2;
}

int FilterChip::rightInset() const
{
    layoutText();
    return width() - contentWidth() - leftInset();
}

QSize FilterChip::sizeHint() const
{

    layoutText();
    const QFontMetrics metrics(font());

    const int w = kPadX * 2 + m_labelAdvance + kGap + m_countAdvance;

    const int h = qMax(Metrics::kBarControlHeight, metrics.height() + 4);
    return QSize(w, h);
}

void FilterChip::enterEvent(QEnterEvent *event)
{

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
    layoutText();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

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

    const int runStart = (width() - m_runWidth) / 2 - m_runLeft;

    painter.setFont(font());
    painter.setPen(textColor);
    painter.drawText(QRectF(runStart, 0, m_countOriginX, height()),
                     Qt::AlignLeft | Qt::AlignVCenter, m_label);

    QColor countColor = m_countColor;
    if (!on)
        countColor = QColor(QStringLiteral("#5f666e"));
    else if (hot)
        countColor = countColor.lighter(112);

    painter.setPen(countColor);
    painter.drawText(QRectF(runStart + m_countOriginX, 0,
                            m_countAdvance + 8, height()),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QString::number(m_count));
}
