#pragma once

#include <QByteArray>
#include <QColor>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <QString>
#include <QtGlobal>

// Lucide-style 24px outline glyphs. They are rasterised once into an alpha mask
// and tinted at paint time, so every glyph inherits the current theme colour and
// cross-fades cleanly with the animated controls.
namespace Icons {

enum class Id {
    Cairn,      // brand mark: stacked stones
    CairnCompact, // the mark with fewer stones, for 16-24px where they merge
    Layout,     // snapshot / frame
    Folder,
    File,
    Archive,
    Executable,
    Image,
    Code,
    Text,
    Link,
    Search,
    Plus,
    Info,
    Grid,
    Close,
    Minimize,
    Maximize,
    Restore
};

const char *svgFor(Id id);

// Rendered at `logicalSize` and scaled by `dpr`; tint it with tint().
QPixmap alphaMask(Id id, int logicalSize, qreal dpr = 1.0);

inline QPixmap tint(const QPixmap &mask, const QColor &color)
{
    QPixmap out = mask;
    QPainter p(&out);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(out.rect(), color);
    p.end();
    return out;
}

// Convenience for one-off glyphs (labels, tooltips, empty states).
QPixmap render(Id id, int logicalSize, const QColor &color, qreal dpr = 1.0);

} // namespace Icons
