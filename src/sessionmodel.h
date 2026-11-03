#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QStringList>

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

QString kindName(Kind kind);
QString kindShortName(Kind kind);
QString kindColor(Kind kind);

struct Entry {
    QString name;
    QString relativePath;
    Kind kind = Kind::Other;
    QString suffix;
    qint64 size = 0;
    QDateTime modified;
    int children = -1;

    QPoint pos;
    QPoint cell;
    bool fromShell = false;

    QString target;

    QJsonObject toJson() const;
    static Entry fromJson(const QJsonObject &object);
};

struct Session {
    QString id;
    QString name;
    QString rootPath;
    QDateTime capturedAt;
    QSize screen;
    QSize cellSize;
    bool livePositions = false;

    QList<Entry> entries;

    int countOf(Kind kind) const;
    qint64 totalSize() const;
    QJsonObject toJson() const;
    static Session fromJson(const QJsonObject &object);

    QString summary() const;
};

namespace KindDetect {

Kind classify(const QString &path, const QString &suffix, bool isDir);

Kind kindForSuffix(const QString &suffix);

}
