#include "sessionstore.h"
#include "desktoppositions.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSettings>
#include <QStandardPaths>
#include <QSet>
#include <algorithm>

namespace {

// Explorer's grid is column-major: it fills a column top to bottom, then moves
// right. Laying computed positions out the same way makes a non-desktop folder
// read like the desktop it is imitating.
constexpr int kMaxRows = 9;

// Items that exist in every desktop folder but are created by Windows, not by
// the user, and that Explorer never displays. They are skipped: a snapshot is
// meant to show the folder as it looks, and these have no icon to record.
bool isHiddenSystemItem(const QString &name)
{
    return name.compare(QLatin1String("desktop.ini"), Qt::CaseInsensitive) == 0
        || name.compare(QLatin1String("Thumbs.db"), Qt::CaseInsensitive) == 0;
}

QString lowerSuffix(const QFileInfo &info)
{
    const QString suffix = info.suffix().toLower();
    return suffix;
}

// Baseline folder count, without walking an entire tree (which would make a
// large folder feel frozen).
int countChildren(const QString &path, int cap = 999)
{
    QDir dir(path);
    if (!dir.exists())
        return 0;
    const QStringList entries = dir.entryList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    return qMin(entries.size(), cap);
}

qint64 directorySize(const QString &path, int maxFiles = 4000)
{
    qint64 total = 0;
    int seen = 0;
    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext() && seen < maxFiles) {
        it.next();
        total += it.fileInfo().size();
        ++seen;
    }
    return total;
}

QString sanitize(const QString &name)
{
    QString cleaned = name;
    const QString invalid = QStringLiteral("\\/:*?\"<>|");
    for (const QChar &c : invalid)
        cleaned.replace(c, QLatin1Char('-'));
    cleaned = cleaned.trimmed();
    return cleaned.isEmpty() ? QStringLiteral("session") : cleaned;
}

} // namespace

namespace SessionStore {

QString directory()
{
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString dir = base + QStringLiteral("/sessions");
    QDir().mkpath(dir);
    return dir;
}

QString desktopPath()
{
    // Ask the shell rather than assuming %USERPROFILE%\Desktop, which is wrong
    // whenever the folder has been redirected (OneDrive does this by default).
    const QStringList candidates = QStandardPaths::standardLocations(
        QStandardPaths::DesktopLocation);
    for (const QString &candidate : candidates) {
        if (QFileInfo(candidate).isDir())
            return QDir::cleanPath(candidate);
    }
    return QDir::cleanPath(candidates.value(0));
}

bool isDesktopPath(const QString &path)
{
    const QFileInfo given(QDir::cleanPath(path));
    const QFileInfo desktop(desktopPath());
    return given.canonicalFilePath() == desktop.canonicalFilePath()
        && !given.canonicalFilePath().isEmpty();
}

QString suggestedName(const QString &rootPath)
{
    const QString folder = QFileInfo(rootPath).fileName();
    return folder.isEmpty() ? QStringLiteral("Session") : folder;
}

QString makeId(const QString &rootPath)
{
    const QString stem = sanitize(QFileInfo(rootPath).fileName().toLower());
    const QString stamp =
        QString::number(QDateTime::currentMSecsSinceEpoch(), 36).right(5);

    QString id = stem + QLatin1Char('-') + stamp;
    // Two captures in the same millisecond are unlikely, but the handle is the
    // file name, so a collision would silently overwrite.
    int attempt = 1;
    while (QFile::exists(pathForId(id)))
        id = stem + QLatin1Char('-') + stamp + QLatin1Char('-')
             + QString::number(attempt++, 36);
    return id;
}

QString pathForId(const QString &id)
{
    return directory() + QLatin1Char('/') + sanitize(id) + QStringLiteral(".session");
}

Session capture(const QString &rootPath)
{
    Session session;
    session.rootPath = QDir::cleanPath(rootPath);
    session.capturedAt = QDateTime::currentDateTime();
    session.name = suggestedName(session.rootPath);
    session.id = makeId(session.rootPath);

    QFileInfo rootInfo(session.rootPath);
    if (!rootInfo.isDir())
        return session;

    const bool desktop = isDesktopPath(session.rootPath);

    // Only the desktop has a live Explorer view to read positions from.
    QHash<QString, QPoint> shellPositions;
    QHash<QString, QString> shellTargets;
    if (desktop) {
        DesktopPositions::Geometry geometry;
        shellPositions = DesktopPositions::read(&shellTargets, &geometry);
        session.livePositions = geometry.live;
        session.screen = geometry.screen;
        session.cellSize = geometry.cell;
    } else {
        const DesktopPositions::Geometry geometry = DesktopPositions::fallbackGeometry();
        session.screen = geometry.screen;
        session.cellSize = geometry.cell;
        session.livePositions = false;
    }

    QDir dir(session.rootPath);
    const QFileInfoList infos = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
        QDir::Name | QDir::IgnoreCase);

    // Stable order for the computed grid: folders first, then files, both
    // alphabetically, which is how a fresh desktop is sorted.
    QList<QFileInfo> ordered;
    ordered.reserve(infos.size());
    for (const QFileInfo &info : infos) {
        if (!isHiddenSystemItem(info.fileName()))
            ordered.append(info);
    }
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const QFileInfo &a, const QFileInfo &b) {
                         if (a.isDir() != b.isDir())
                             return a.isDir();
                         return a.fileName().compare(b.fileName(), Qt::CaseInsensitive) < 0;
                     });

    // Cells already taken by a live position, so a computed tile never lands on
    // top of one read from the desktop.
    QSet<int> usedCells;
    const QSize cellSize = session.cellSize.isValid() ? session.cellSize
                                                      : QSize(75, 98);
    const int cellW = qMax(1, cellSize.width());
    const int cellH = qMax(1, cellSize.height());

    const auto cellKey = [](int col, int row) { return col * 10000 + row; };

    // Seed occupancy from the positions the shell reported for entries that are
    // actually in this folder. Explorer hides a known extension, so the shell's
    // key is the file name with its final extension removed ("SnipEx_v1.4.34.zip"
    // is reported as "SnipEx_v1.4.34"), with folders reported unchanged.
    const auto shellPositionFor = [&](const QFileInfo &info, QPoint *out) {
        auto exact = shellPositions.constFind(info.fileName());
        if (exact != shellPositions.constEnd()) {
            *out = exact.value();
            return true;
        }
        auto trimmed = shellPositions.constFind(info.completeBaseName());
        if (trimmed != shellPositions.constEnd()) {
            *out = trimmed.value();
            return true;
        }
        return false;
    };

    for (const QFileInfo &info : ordered) {
        QPoint position;
        if (shellPositionFor(info, &position))
            usedCells.insert(cellKey(position.x() / cellW, position.y() / cellH));
    }

    int gridSlot = 0;
    for (const QFileInfo &info : ordered) {
        Entry entry;
        entry.name = info.fileName();
        entry.relativePath = entry.name;
        entry.suffix = info.isDir() ? QStringLiteral("dir") : lowerSuffix(info);
        entry.kind = KindDetect::classify(info.filePath(), entry.suffix, info.isDir());
        entry.size = info.isDir() ? directorySize(info.filePath()) : info.size();
        entry.modified = info.lastModified();

        if (info.isDir())
            entry.children = countChildren(info.filePath());

        // Match the shell by full name first, then by the extension-less name it
        // actually reports.
        QPoint shellPos;
        if (shellPositionFor(info, &shellPos)) {
            entry.pos = shellPos;
            entry.fromShell = true;
            entry.cell = QPoint(shellPos.x() / cellW, shellPos.y() / cellH);
        } else {
            // First free cell, filling columns top-to-bottom like Explorer.
            int slot = gridSlot;
            QPoint cell;
            do {
                cell = QPoint(slot / kMaxRows, slot % kMaxRows);
                ++slot;
            } while (usedCells.contains(cellKey(cell.x(), cell.y())));
            gridSlot = slot;

            entry.fromShell = false;
            entry.cell = cell;
            // A matching pixel position keeps both kinds of tile in one
            // coordinate space when the map draws them.
            entry.pos = QPoint(cell.x() * cellW, cell.y() * cellH);
        }

        if (info.isSymLink())
            entry.target = info.symLinkTarget();
        else if (entry.kind == Kind::Shortcut && shellTargets.contains(entry.name))
            entry.target = shellTargets.value(entry.name);
        else if (entry.kind == Kind::Shortcut)
            entry.target = DesktopPositions::resolveShortcut(info.filePath());

        session.entries.append(entry);
    }

    return session;
}

bool save(const Session &session, QString *error)
{
    QDir().mkpath(directory());
    const QString path = pathForId(session.id);

    QJsonDocument document(session.toJson());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    if (file.write(document.toJson(QJsonDocument::Indented)) < 0) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.close();

    // Remember the most recent handle so the viewer can offer it first.
    QSettings settings;
    settings.setValue(QStringLiteral("sessions/lastId"), session.id);
    return true;
}

Session load(const QString &path, bool *ok)
{
    if (ok)
        *ok = false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return {};

    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("format")).toString()
        != QLatin1String("cairn.session"))
        return {};

    Session session = Session::fromJson(object);
    if (session.id.isEmpty())
        session.id = QFileInfo(path).completeBaseName();
    if (session.name.isEmpty())
        session.name = session.id;

    if (ok)
        *ok = true;
    return session;
}

QList<Session> listAll(const QStringList &paths)
{
    QStringList files = paths;
    if (files.isEmpty()) {
        QDir dir(directory());
        files = dir.entryList({QStringLiteral("*.session")},
                              QDir::Files, QDir::Time);
        for (QString &name : files)
            name = dir.filePath(name);
    }

    QList<Session> sessions;
    for (const QString &path : files) {
        bool ok = false;
        const Session session = load(path, &ok);
        if (ok)
            sessions.append(session);
    }

    std::sort(sessions.begin(), sessions.end(),
              [](const Session &a, const Session &b) {
                  return a.capturedAt > b.capturedAt;
              });
    return sessions;
}

} // namespace SessionStore
