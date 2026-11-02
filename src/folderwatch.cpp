#include "folderwatch.h"
#include "desktoppositions.h"
#include "sessionstore.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QTimer>

namespace {
// A rename or delete arrives as several notifications in a burst; one settled
// re-scan is much cheaper than one per notification.
constexpr int kDebounceMs = 250;

// How often the desktop's icon positions are re-read. Dragging an icon is a
// deliberate, human-paced action, so a poll every second reads as immediate
// without burning a shell call more often than needed.
constexpr int kPositionPollMs = 1000;
}

FolderWatcher::FolderWatcher(QObject *parent)
    : QObject(parent)
{
    m_watcher = new QFileSystemWatcher(this);

    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(kDebounceMs);
    connect(m_debounce, &QTimer::timeout, this, &FolderWatcher::onNotification);

    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
            [this](const QString &) { arm(); });

    m_positionPoll = new QTimer(this);
    m_positionPoll->setInterval(kPositionPollMs);
    connect(m_positionPoll, &QTimer::timeout, this, &FolderWatcher::pollPositions);
}

void FolderWatcher::watch(const QString &path)
{
    const QString clean = QDir::cleanPath(path);

    if (clean == m_path && isWatching())
        return;

    stop();
    m_path = clean;

    if (m_path.isEmpty() || !QFileInfo(m_path).isDir())
        return;

    m_watcher->addPath(m_path);

    // Dragging an icon changes no file, so on the desktop the filesystem
    // watcher alone would miss it. Take a baseline now and compare later; the
    // first comparison must not report a change, or every open would look like
    // one.
    if (SessionStore::isDesktopPath(m_path)) {
        DesktopPositions::Geometry geometry;
        m_lastPositions = DesktopPositions::read(nullptr, &geometry);
        m_hasSnapshot = true;
        m_positionPoll->start();
    }
}

void FolderWatcher::stop()
{
    if (m_debounce->isActive())
        m_debounce->stop();
    m_positionPoll->stop();

    const QStringList watched = m_watcher->directories();
    if (!watched.isEmpty())
        m_watcher->removePaths(watched);

    m_lastPositions.clear();
    m_hasSnapshot = false;
    m_path.clear();
}

bool FolderWatcher::isWatching() const
{
    return !m_path.isEmpty() && m_watcher->directories().contains(m_path);
}

bool FolderWatcher::isPollingPositions() const
{
    return m_positionPoll && m_positionPoll->isActive();
}

void FolderWatcher::arm()
{
    // Restarting the timer collapses a burst into a single report.
    m_debounce->start();
}

void FolderWatcher::pollPositions()
{
    DesktopPositions::Geometry geometry;
    const QHash<QString, QPoint> now = DesktopPositions::read(nullptr, &geometry);

    if (!m_hasSnapshot) {
        m_lastPositions = now;
        m_hasSnapshot = true;
        return;
    }

    // Cheap equality first: the common case is that nothing moved.
    if (now == m_lastPositions)
        return;

    m_lastPositions = now;
    emit folderChanged();
}

void FolderWatcher::onNotification()
{
    // The operating system drops the watch when the folder is deleted or
    // renamed away, so it has to be re-added by hand. Otherwise the app would
    // silently stop updating after the user moved the folder it was showing.
    if (QFileInfo(m_path).isDir()) {
        if (!m_watcher->directories().contains(m_path) && !m_path.isEmpty())
            m_watcher->addPath(m_path);
    } else {
        // Gone: report it once and leave the watch off. The window shows the
        // missing-folder state rather than a stale map.
        m_watcher->removePaths(m_watcher->directories());
        m_positionPoll->stop();
    }

    // A file-level change can also move icons (creating a file reflows the
    // grid), so refresh the baseline to avoid reporting the same change twice.
    if (m_positionPoll->isActive()) {
        DesktopPositions::Geometry geometry;
        m_lastPositions = DesktopPositions::read(nullptr, &geometry);
    }

    emit folderChanged();
}
