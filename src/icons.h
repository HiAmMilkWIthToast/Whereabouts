#pragma once

#include <QByteArray>
#include <QColor>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <QString>
#include <QtGlobal>

namespace Icons {

enum class Id {
    Cairn,
    CairnCompact,
    Layout,
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
    List,
    Home,
    Export,
    SortAsc,
    SortDesc,
    Close,
    Minimize,
    Maximize,
    Restore
};

const char *svgFor(Id id);

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

QPixmap render(Id id, int logicalSize, const QColor &color, qreal dpr = 1.0);

}
