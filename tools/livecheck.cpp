// End-to-end checks for live folder updates. Development helper only.
//
// Uses a real folder on disk: the point of the feature is that moving something
// in Explorer updates the app, so the test must exercise the filesystem rather
// than a stub.
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileSystemWatcher>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

#include "filterchip.h"
#include "folderwatch.h"
#include "layoutview.h"
#include "mainwindow.h"
#include "sessionmodel.h"
#include "sessionstore.h"

static QTextStream out(stdout);
static int failures = 0;

static void check(bool ok, const QString &what)
{
    out << (ok ? "  PASS  " : "  FAIL  ") << what << "\n";
    if (!ok)
        ++failures;
}

// Runs the event loop for a while so the watcher's debounce timer can fire.
static void pump(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

static void writeFile(const QString &path, const QByteArray &body)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(body);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CairnLiveTest"));
    app.setOrganizationName(QStringLiteral("CairnLiveTest"));
    app.setStyleSheet(QString());

    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        out << "could not create a temp folder\n";
        return 1;
    }
    const QString root = tmp.path();

    // ---------------------------------------------------------------- chip text
    {
        out << "== chip shows name and count separately ==\n";
        FilterChip chip(QStringLiteral("Folder"), 1);
        chip.resize(chip.sizeHint());
        check(chip.label() == QStringLiteral("Folder"),
              QStringLiteral("label is just the kind name (%1)").arg(chip.label()));
        check(chip.count() == 1,
              QStringLiteral("count is carried separately (%1)").arg(chip.count()));
        // An odd leftover cannot be split evenly across two sides, so a single
        // pixel is the smallest gap any correct layout can leave. Demanding
        // exact equality only held while the content width happened to be even.
        const int gap = qAbs(chip.leftInset() - chip.rightInset());
        check(gap <= 1,
              QStringLiteral("padding is symmetric to within a pixel (L=%1 R=%2)")
                  .arg(chip.leftInset())
                  .arg(chip.rightInset()));
        check(chip.width() > 0 && chip.height() > 0,
              QStringLiteral("sizeHint is sane (%1x%2)")
                  .arg(chip.width())
                  .arg(chip.height()));
    }

    // ------------------------------------------------------- the watcher alone
    {
        out << "\n== watcher sees a new file ==\n";
        writeFile(root + QStringLiteral("/first.txt"), "a");

        FolderWatcher watcher;
        int reports = 0;
        QObject::connect(&watcher, &FolderWatcher::folderChanged,
                         [&] { ++reports; });

        watcher.watch(root);
        check(watcher.isWatching(), QStringLiteral("watching the folder"));

        writeFile(root + QStringLiteral("/added.txt"), "b");
        pump(900);
        check(reports > 0, QStringLiteral("a new file raised a change (%1)").arg(reports));

        // A burst must not raise one signal per notification.
        reports = 0;
        for (int i = 0; i < 5; ++i)
            writeFile(root + QStringLiteral("/burst%1.txt").arg(i), "c");
        pump(900);
        check(reports >= 1 && reports <= 2,
              QStringLiteral("a burst collapses into 1-2 reports (%1)").arg(reports));

        watcher.stop();
        check(!watcher.isWatching(), QStringLiteral("stop() releases the watch"));
    }

    // ------------------------------------------- the window follows its folder
    {
        out << "\n== the open snapshot follows the folder ==\n";

        // Start from a clean, known folder.
        for (const QString &name : QDir(root).entryList(QDir::Files | QDir::NoDotAndDotDot))
            QFile::remove(root + QLatin1Char('/') + name);

        writeFile(root + QStringLiteral("/keep.txt"), "hello");
        writeFile(root + QStringLiteral("/move-me.zip"), "archive");

        MainWindow window;
        window.resize(1000, 640);
        Session session = SessionStore::capture(root);
        window.presentSession(session);
        window.show();
        pump(300);

        auto *view = window.findChild<LayoutView *>();
        check(view != nullptr, QStringLiteral("layout view is reachable"));
        if (!view)
            return 1;

        const int before = view->visiblePaths().size();
        check(before == 2, QStringLiteral("two items to begin with (%1)").arg(before));

        // Add a third item while the window is open.
        writeFile(root + QStringLiteral("/arrived.png"), "img");
        pump(1200);

        const int after = view->visiblePaths().size();
        check(after == 3,
              QStringLiteral("the new file appears without a manual refresh (%1 -> %2)")
                  .arg(before).arg(after));

        bool seesIt = false;
        for (const QString &p : view->visiblePaths()) {
            if (p == QStringLiteral("arrived.png"))
                seesIt = true;
        }
        check(seesIt, QStringLiteral("the new item is the one that was added"));

        // The handle must NOT change: this is the same snapshot, following.
        check(window.findChild<LayoutView *>() != nullptr,
              QStringLiteral("window still shows a session"));

        // Removing an item must also be followed.
        QFile::remove(root + QStringLiteral("/move-me.zip"));
        pump(1200);
        check(view->visiblePaths().size() == 2,
              QStringLiteral("a deleted file disappears (%1)")
                  .arg(view->visiblePaths().size()));
    }

    // ----------------------------------------------- a vanished folder is safe
    {
        out << "\n== deleting the folder does not wipe the view ==\n";
        QTemporaryDir doomed;
        const QString doomedRoot = doomed.path();
        writeFile(doomedRoot + QStringLiteral("/only.txt"), "x");

        MainWindow window;
        Session session = SessionStore::capture(doomedRoot);
        window.presentSession(session);
        window.show();
        pump(300);

        auto *view = window.findChild<LayoutView *>();
        const int before = view->visiblePaths().size();
        check(before == 1, QStringLiteral("one item before the folder is removed"));

        doomed.remove();
        pump(1200);

        // The map must still show what it had rather than blanking out, which is
        // what a naive re-scan of a missing folder would do.
        check(view->visiblePaths().size() == 1,
              QStringLiteral("the map keeps its items (%1)")
                  .arg(view->visiblePaths().size()));
    }

    out << "\n" << (failures == 0 ? "LIVE PASSED\n" : "LIVE FAILED\n");
    out.flush();
    return failures == 0 ? 0 : 1;
}
