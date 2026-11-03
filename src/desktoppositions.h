#pragma once

#include <QHash>
#include <QPoint>
#include <QSize>
#include <QString>

namespace DesktopPositions {

struct Geometry {
    QSize screen;
    QSize cell;
    bool live = false;
};

QHash<QString, QPoint> read(QHash<QString, QString> *targets, Geometry *geometry);

Geometry fallbackGeometry();

QString resolveShortcut(const QString &path);

}
