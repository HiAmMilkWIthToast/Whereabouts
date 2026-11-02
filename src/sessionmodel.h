#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QStringList>

// What an entry is, judged from its extension and then confirmed against its
// magic bytes. "Archive" and "Executable" are the two the app calls out
// explicitly, since a .zip and a .exe are what people most want to spot while
// scanning a desktop.
enum class Kind {
    Folder,
    Archive,
    Executable,
    Document,
    Image,
    Code,
    Audio,
    Video,
    Shortcut,
    Other
};

QString kindName(Kind kind);       // "Archive", "Executable", ...
QString kindShortName(Kind kind);  // "zip", "exe", "dir", ...
QString kindColor(Kind kind);      // hex colour used by the map and tree

// One captured item.
struct Entry {
    QString name;
    QString relativePath;   // path below the snapshot root, '/' separated
    Kind kind = Kind::Other;
    QString suffix;         // lower-case extension without the dot, "dir" for folders
    qint64 size = 0;
    QDateTime modified;
    int children = -1;      // -1 = unknown/not a folder

    // Position as reported by Explorer, in desktop pixels. Only meaningful when
    // fromShell is true; otherwise the grid cell was assigned by this app.
    QPoint pos;
    QPoint cell;
    bool fromShell = false;

    QString target;         // resolved .lnk target, when available

    QJsonObject toJson() const;
    static Entry fromJson(const QJsonObject &object);
};

// A whole snapshot. The id is the human-facing handle: it names the .session
// file and is what someone types into the viewer window.
struct Session {
    QString id;
    QString name;           // friendly label, defaults to the folder name
    QString rootPath;
    QDateTime capturedAt;
    QSize screen;           // desktop extent the positions were taken from
    QSize cellSize;         // Explorer's icon cell, used to lay out a map
    bool livePositions = false; // true when positions came from the real desktop

    QList<Entry> entries;

    int countOf(Kind kind) const;
    qint64 totalSize() const;
    QJsonObject toJson() const;
    static Session fromJson(const QJsonObject &object);

    // "4 folders · 7 files · 3 archives", for headers and cards.
    QString summary() const;
};

namespace KindDetect {

// Extension first, then a short read of the file's first bytes. `open` is only
// touched when the extension is ambiguous, so scanning a folder stays cheap.
Kind classify(const QString &path, const QString &suffix, bool isDir);

// Maps an extension to a kind without touching the disk.
Kind kindForSuffix(const QString &suffix);

} // namespace KindDetect
