#pragma once

#include <QColor>
#include <QString>
#include <QtGlobal>

// One accent, one neutral ramp, one stylesheet. Everything painted by hand and
// everything styled by QSS reads the same values, so the app stays coherent.
namespace Theme {

QColor accent();
void setAccent(const QColor &color);

QString accentName();
QString accentDimName();
QString accentHoverName();
QString accentContrastName();

// Built from the accent, so a colour change can be applied at runtime.
QString buildStyleSheet();

QString formatBytes(qint64 bytes);

inline QColor lerpColor(const QColor &a, const QColor &b, qreal t)
{
    t = qBound<qreal>(0.0, t, 1.0);
    return QColor(qRound(a.red() + (b.red() - a.red()) * t),
                  qRound(a.green() + (b.green() - a.green()) * t),
                  qRound(a.blue() + (b.blue() - a.blue()) * t),
                  qRound(a.alpha() + (b.alpha() - a.alpha()) * t));
}

} // namespace Theme

// Shared metrics. Compact on purpose: the whole UI is meant to read as a small,
// dense instrument rather than a web page in a frame.
namespace Metrics {
constexpr int kTitleBarHeight = 36;
constexpr int kSidebarWidth = 46;
constexpr int kStatusBarHeight = 24;
constexpr int kPagePad = 22;
constexpr int kInspectorWidth = 238;
} // namespace Metrics
