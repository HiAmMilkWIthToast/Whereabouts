#include "filterchip.h"
#include "theme.h"

#include <QFontMetrics>
#include <QImage>
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
    const QString countText = QString::number(m_count);
    m_labelAdvance = metrics.horizontalAdvance(m_label);
    m_countAdvance = metrics.horizontalAdvance(countText);
    m_countOriginX = m_labelAdvance + kGap;

    // Where the two runs land is decided by the rasteriser, not by the metrics
    // API: a glyph's advance includes bearings that draw nothing, and predicting
    // the painted extent from the metrics still left the text visibly off
    // centre. So the run is drawn once, in black on transparent, and the pixels
    // that actually appear are measured. That is the same rasteriser the paint
    // code uses, so the measurement cannot disagree with what gets drawn.
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
        // Nothing measurable, so fall back to the advance widths rather than
        // collapsing the run to nothing.
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
    // Measured every time rather than cached: the first call can happen before
    // the widget's font is resolved, and a cached zero-width result would then
    // stick for the life of the chip.
    layoutText();
    const QFontMetrics metrics(font());
    // Sized from the advance widths, which include the bearings the ink does
    // not fill. That leaves a little breathing room at both ends, and because
    // the run is centred inside this width the two ends stay equal.
    const int w = kPadX * 2 + m_labelAdvance + kGap + m_countAdvance;
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
    layoutText();

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

    // The run is centred on the pixels it will paint, so the gap either side of
    // the text is equal. runLeft cancels the bearing the first glyph carries, so
    // the ink starts exactly where the measurement said it would.
    const int runStart = (width() - m_runWidth) / 2 - m_runLeft;

    painter.setFont(font());
    painter.setPen(textColor);
    painter.drawText(QRectF(runStart, 0, m_countOriginX, height()),
                     Qt::AlignLeft | Qt::AlignVCenter, m_label);

    // The count is dimmer while switched on, and muted further when off, so it
    // never competes with the label it belongs to.
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
