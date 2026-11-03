#pragma once

#include <QColor>
#include <QString>
#include <QtGlobal>

// One accent, one neutral ramp, one stylesheet. Everything painted by hand and
// everything styled by QSS reads the same values, so the app stays coherent.
namespace Theme {

QColor accent();

// The mark's own colour. The accent is the app's interactive colour and is used
// by chips, sliders and focus rings; the logo is not one of those, and dressing
// it in the same blue made it read as another control. Gold is the tone the kind
// colours have used for archives since the first release, so it is already at
// home in this palette.
QColor brand();
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
// One height for every control on a horizontal bar -- chips, fields, buttons.
// Sized separately they drifted apart (a chip came out 21px against a 26px
// button), and two rows of controls that disagree by five pixels read as
// misaligned even when their centres match.
constexpr int kBarControlHeight = 24;
} // namespace Metrics
