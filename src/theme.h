#pragma once

#include <QColor>
#include <QString>
#include <QtGlobal>

namespace Theme {

QColor accent();

QColor brand();
QString accentName();
QString accentDimName();
QString accentHoverName();
QString accentContrastName();

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

}

namespace Metrics {
constexpr int kTitleBarHeight = 36;
constexpr int kSidebarWidth = 46;
constexpr int kStatusBarHeight = 24;
constexpr int kPagePad = 22;
constexpr int kInspectorWidth = 238;

constexpr int kBarControlHeight = 24;
}
