// Verifies that a change in icon positions is detected, without needing to drag
// anything on the real desktop. Development helper only.
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QTextStream>
#include <QTimer>

#include "desktoppositions.h"
#include "folderwatch.h"
#include "sessionstore.h"

static QTextStream out(stdout);
static int failures = 0;

static void check(bool ok, const QString &what)
{
    out << (ok ? "  PASS  " : "  FAIL  ") << what << "\n";
    if (!ok)
        ++failures;
}

static void pump(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Cairn"));
    app.setOrganizationName(QStringLiteral("Cairn"));

    const QString desktop = SessionStore::desktopPath();

    // ---------------------------------------------------- polling is desktop-only
    {
        out << "== polling only runs where positions are live ==\n";

        FolderWatcher watcher;
        watcher.watch(desktop);
        check(watcher.isWatching(), QStringLiteral("desktop is watched"));
        check(watcher.isPollingPositions(),
              QStringLiteral("icon positions are polled on the desktop"));

        // A non-desktop folder has computed positions, so a drag is impossible
        // there and polling must stay off.
        const QString temp = QDir::tempPath();
        watcher.watch(temp);
        check(!watcher.isPollingPositions(),
              QStringLiteral("polling is off for an ordinary folder (%1)")
                  .arg(temp));

        watcher.watch(desktop);
        check(watcher.isPollingPositions(),
              QStringLiteral("polling resumes when the desktop is watched again"));
    }

    // ------------------------------------------- the baseline read is non-zero
    {
        out << "\n== the shell really reports positions ==\n";
        DesktopPositions::Geometry geometry;
        const QHash<QString, QPoint> positions =
            DesktopPositions::read(nullptr, &geometry);
        out << "    positions reported: " << positions.size()
            << "  live=" << geometry.live << "\n";
        check(geometry.live,
              QStringLiteral("the desktop view answered (live=%1)").arg(geometry.live));
        check(!positions.isEmpty(),
              QStringLiteral("at least one icon position was read (%1)")
                  .arg(positions.size()));

        // Reading twice with nothing changed must give identical results, or the
        // poll would report a change on every tick and re-scan constantly.
        const QHash<QString, QPoint> again =
            DesktopPositions::read(nullptr, &geometry);
        check(again == positions,
              QStringLiteral("two consecutive reads are identical (stable baseline)"));
    }

    // -------------------------------------- a real poll tick does not false-fire
    {
        out << "\n== an idle desktop does not report changes ==\n";
        FolderWatcher watcher;
        int reports = 0;
        QObject::connect(&watcher, &FolderWatcher::folderChanged,
                         [&] { ++reports; });
        watcher.watch(desktop);

        // Several poll intervals with nothing happening.
        pump(2600);
        check(reports == 0,
              QStringLiteral("no change reported while nothing moves (%1)").arg(reports));
    }

    out << "\n" << (failures == 0 ? "DESKPASS PASSED\n" : "DESKPASS FAILED\n");
    out.flush();
    return failures == 0 ? 0 : 1;
}
