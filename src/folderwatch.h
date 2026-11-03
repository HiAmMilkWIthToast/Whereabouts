#pragma once

#include <QHash>
#include <QObject>
#include <QPoint>
#include <QString>

class QFileSystemWatcher;
class QTimer;

class FolderWatcher : public QObject
{
    Q_OBJECT
public:
    explicit FolderWatcher(QObject *parent = nullptr);

    void watch(const QString &path);
    void stop();

    bool isWatching() const;
    QString path() const { return m_path; }

    bool isPollingPositions() const;

signals:

    void folderChanged();

private:
    void onNotification();
    void arm();
    void pollPositions();

    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_debounce = nullptr;
    QTimer *m_positionPoll = nullptr;
    QString m_path;

    QHash<QString, QPoint> m_lastPositions;
    bool m_hasSnapshot = false;
};
