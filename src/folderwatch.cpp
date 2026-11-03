#include "folderwatch.h"
#include "desktoppositions.h"
#include "sessionstore.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QTimer>

namespace {

constexpr int kDebounceMs = 250;

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

    if (now == m_lastPositions)
        return;

    m_lastPositions = now;
    emit folderChanged();
}

void FolderWatcher::onNotification()
{

    if (QFileInfo(m_path).isDir()) {
        if (!m_watcher->directories().contains(m_path) && !m_path.isEmpty())
            m_watcher->addPath(m_path);
    } else {

        m_watcher->removePaths(m_watcher->directories());
        m_positionPoll->stop();
    }

    if (m_positionPoll->isActive()) {
        DesktopPositions::Geometry geometry;
        m_lastPositions = DesktopPositions::read(nullptr, &geometry);
    }

    emit folderChanged();
}
