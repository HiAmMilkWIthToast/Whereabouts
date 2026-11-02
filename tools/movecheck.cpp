// Reproduces the reported case: a file is MOVED while the window is open.
// Development helper only.
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

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

static QStringList paths(LayoutView *v)
{
    return v->visiblePaths();
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CairnMoveTest"));
    app.setOrganizationName(QStringLiteral("CairnMoveTest"));

    QTemporaryDir tmp;
    QTemporaryDir elsewhere;
    const QString root = tmp.path();
    const QString away = elsewhere.path();

    writeFile(root + "/notes.txt", "hello");
    writeFile(root + "/other.txt", "world");

    MainWindow window;
    window.resize(1000, 640);
    window.presentSession(SessionStore::capture(root));
    window.show();
    pump(400);

    auto *view = window.findChild<LayoutView *>();
    check(view != nullptr, QStringLiteral("view is up"));
    if (!view)
        return 1;

    check(paths(view).size() == 2, QStringLiteral("starts with 2 items (%1)").arg(paths(view).size()));

    // ---- THE REPORTED CASE 1: rename within the folder
    out << "\n== rename a .txt in place ==\n";
    QFile::rename(root + "/notes.txt", root + "/renamed.txt");
    pump(1200);
    QStringList after = paths(view);
    check(after.size() == 2, QStringLiteral("still 2 items (%1)").arg(after.size()));
    check(after.contains(QStringLiteral("renamed.txt")),
          QStringLiteral("shows the new name (%1)").arg(after.join(QLatin1Char(','))));
    check(!after.contains(QStringLiteral("notes.txt")),
          QStringLiteral("old name is gone"));

    // ---- CASE 2: move a .txt OUT of the folder
    out << "\n== move a .txt out of the folder ==\n";
    QFile::rename(root + "/renamed.txt", away + "/renamed.txt");
    pump(1200);
    after = paths(view);
    check(after.size() == 1, QStringLiteral("down to 1 item (%1)").arg(after.size()));
    check(!after.contains(QStringLiteral("renamed.txt")),
          QStringLiteral("the moved file disappeared (%1)").arg(after.join(QLatin1Char(','))));

    // ---- CASE 3: move a .txt INTO the folder
    out << "\n== move a .txt into the folder ==\n";
    QFile::rename(away + "/renamed.txt", root + "/incoming.txt");
    pump(1200);
    after = paths(view);
    check(after.size() == 2, QStringLiteral("back to 2 items (%1)").arg(after.size()));
    check(after.contains(QStringLiteral("incoming.txt")),
          QStringLiteral("the arriving file shows up (%1)").arg(after.join(QLatin1Char(','))));

    // ---- CASE 4: move a .txt into a subfolder of the folder
    out << "\n== move a .txt into a subfolder ==\n";
    QDir().mkpath(root + "/sub");
    pump(1200);
    const int withSub = paths(view).size();
    QFile::rename(root + "/incoming.txt", root + "/sub/incoming.txt");
    pump(1200);
    after = paths(view);
    check(!after.contains(QStringLiteral("incoming.txt")),
          QStringLiteral("a file moved into a subfolder leaves the top level (%1)")
              .arg(after.join(QLatin1Char(','))));
    out << "    (present before: " << withSub << ", after: " << after.size() << ")\n";

    // ---- CASE 5: the folder itself is renamed on disk
    out << "\n== the watched folder is renamed away ==\n";
    const QString movedRoot = root + "-moved";
    QDir().rename(root, movedRoot);
    pump(1400);
    after = paths(view);
    check(after.size() >= 1,
          QStringLiteral("the map does not blank out when its folder moves (%1)")
              .arg(after.size()));

    QDir().rename(movedRoot, root);

    out << "\n" << (failures == 0 ? "MOVE PASSED\n" : "MOVE FAILED\n");
    out.flush();
    return failures == 0 ? 0 : 1;
}
