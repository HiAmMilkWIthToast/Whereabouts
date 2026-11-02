#pragma once

#include <QHash>
#include <QObject>
#include <QPoint>
#include <QString>

class QFileSystemWatcher;
class QTimer;

// Watches one folder and reports that something about it changed.
//
// Two different kinds of change matter, and they need two different mechanisms:
//
//   * Files appearing, disappearing or being renamed. These raise filesystem
//     notifications, so a file watcher catches them for free.
//   * Icons being dragged. This changes no file at all: Explorer keeps icon
//     positions in the registry, invisible to any watcher. The only way to see
//     it is to ask the shell where the icons are every so often and compare.
//
// The polling half only runs when the watched folder is the desktop, because
// that is the only folder whose icon positions came from Explorer. Every other
// folder is laid out on a computed grid, where a drag cannot happen at all, so
// polling it would burn cycles to discover nothing.
//
// Only the folder itself is watched, not its whole tree: the map shows the
// items directly inside the root, so a subfolder's contents cannot alter it,
// and one watch handle keeps this cheap on large trees.
class FolderWatcher : public QObject
{
    Q_OBJECT
public:
    explicit FolderWatcher(QObject *parent = nullptr);

    // Starts watching `path`, replacing any previous folder. An empty or
    // unreadable path simply leaves the watcher idle.
    void watch(const QString &path);
    void stop();

    bool isWatching() const;
    QString path() const { return m_path; }

    // True when icon positions are being polled rather than only files.
    bool isPollingPositions() const;

signals:
    // Emitted after the debounce delay, once per burst of notifications.
    void folderChanged();

private:
    void onNotification();
    void arm();
    void pollPositions();

    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_debounce = nullptr;
    QTimer *m_positionPoll = nullptr;
    QString m_path;

    // Last observed icon positions, keyed by display name. Compared on every
    // poll so a drag is distinguished from a poll that saw nothing new.
    QHash<QString, QPoint> m_lastPositions;
    bool m_hasSnapshot = false;
};
