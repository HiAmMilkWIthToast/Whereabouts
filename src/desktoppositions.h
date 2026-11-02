#pragma once

#include <QHash>
#include <QPoint>
#include <QSize>
#include <QString>

// Real desktop geometry and icon positions, read from Explorer's own view.
//
// The desktop is special: Explorer owns a list view for it, and that view knows
// exactly where each icon was dragged to. Any other folder usually has no
// Explorer view open, so there is nothing to read and the caller lays the
// entries out on a grid instead. Both cases are represented honestly: positions
// read from the shell are marked live, computed ones are not.
namespace DesktopPositions {

struct Geometry {
    QSize screen;          // extent available to the desktop, in pixels
    QSize cell;            // icon cell Explorer spaces items by
    bool live = false;     // true when the shell view answered
};

// Positions keyed by display name, as Explorer shows it. Only files and folders
// on the desktop itself are returned.
QHash<QString, QPoint> read(QHash<QString, QString> *targets, Geometry *geometry);

// Defaults used when the shell view cannot be reached.
Geometry fallbackGeometry();

// Resolves a .lnk to its target path. Empty when it cannot be resolved.
QString resolveShortcut(const QString &path);

} // namespace DesktopPositions
