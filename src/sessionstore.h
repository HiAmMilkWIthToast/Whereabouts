#pragma once

#include "sessionmodel.h"

#include <QDateTime>
#include <QString>
#include <QStringList>

// Captures a folder into a Session and manages the .session files on disk.
//
// Two different things are stored, and the difference matters:
//   * the folder's layout — where each item sits, derived from the real desktop
//     when the folder is the desktop, otherwise from a stable grid;
//   * metadata about each item — kind, size, timestamp.
//
// File *contents* are deliberately not copied. This is an index of a layout, not
// a backup, so a snapshot of a 40 GB folder costs a few hundred kilobytes.
namespace SessionStore {

// Scans `rootPath`. The desktop's real icon positions are used when the folder
// is the desktop; any other folder is laid out on a grid so the map still reads
// like a desktop.
Session capture(const QString &rootPath);

// Where .session files live. Created on demand.
QString directory();

bool save(const Session &session, QString *error = nullptr);
Session load(const QString &path, bool *ok = nullptr);

QString pathForId(const QString &id);

// Newest first. Unreadable files are skipped rather than reported, so one bad
// file cannot hide the rest of the library.
QList<Session> listAll(const QStringList &paths = {});

// Default names offered for a new session.
QString suggestedName(const QString &rootPath);

// A short, readable, collision-free handle: the folder's name plus a base-36
// stamp, e.g. "desktop-1k4f2a". Doubles as the file name.
QString makeId(const QString &rootPath);

// True when `path` is the user's desktop folder.
bool isDesktopPath(const QString &path);

QString desktopPath();

} // namespace SessionStore
