// End-to-end check of the session pipeline without a visible window:
// capture the desktop, save it, reload it, and confirm the map can lay it out.
// Development helper only.
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSet>
#include <QTextStream>

#include "layoutview.h"
#include "sessionstore.h"

static QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

static int failures = 0;

static void check(bool condition, const QString &what)
{
    out() << (condition ? "  PASS  " : "  FAIL  ") << what << "\n";
    if (!condition)
        ++failures;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CairnE2E"));
    app.setOrganizationName(QStringLiteral("CairnE2E"));

    out() << "== capture the real desktop ==\n";
    const Session captured = SessionStore::capture(SessionStore::desktopPath());
    check(!captured.id.isEmpty(), QStringLiteral("session has a handle: ") + captured.id);
    check(captured.id.startsWith(QStringLiteral("desktop-")),
          QStringLiteral("handle is derived from the folder name"));
    check(captured.livePositions, QStringLiteral("real desktop positions were read"));
    check(!captured.entries.isEmpty(), QStringLiteral("entries were captured"));
    check(captured.screen.width() > 0, QStringLiteral("screen extent was recorded"));
    check(captured.cellSize.width() > 0, QStringLiteral("icon cell was recorded"));

    int live = 0;
    for (const Entry &entry : captured.entries) {
        if (entry.fromShell)
            ++live;
    }
    check(live > 0, QStringLiteral("%1 of %2 entries have live positions")
                        .arg(live).arg(captured.entries.size()));

    // Kinds must reflect content, not just names.
    bool hasArchive = false;
    bool hasExecutable = false;
    for (const Entry &entry : captured.entries) {
        if (entry.kind == Kind::Archive)
            hasArchive = true;
        if (entry.kind == Kind::Executable)
            hasExecutable = true;
    }
    out() << "  info  archives=" << hasArchive << " executables=" << hasExecutable
          << "\n";

    out() << "\n== save and reload ==\n";
    QString error;
    const bool saved = SessionStore::save(captured, &error);
    check(saved, QStringLiteral("save succeeded %1").arg(error));

    const QString path = SessionStore::pathForId(captured.id);
    check(QFile::exists(path), QStringLiteral("file exists at ") + path);

    const qint64 bytes = QFileInfo(path).size();
    out() << "  info  file size " << bytes << " bytes for "
          << captured.entries.size() << " entries\n";
    check(bytes > 0 && bytes < 512 * 1024,
          QStringLiteral("file is small (a map, not a backup)"));

    bool ok = false;
    const Session loaded = SessionStore::load(path, &ok);
    check(ok, QStringLiteral("reload succeeded"));
    check(loaded.id == captured.id, QStringLiteral("handle survives the round trip"));
    check(loaded.entries.size() == captured.entries.size(),
          QStringLiteral("entry count survives"));
    check(loaded.livePositions == captured.livePositions,
          QStringLiteral("layout source survives"));

    // The file must be self-describing, never a copy of the folder's contents.
    QFile raw(path);
    if (!raw.open(QIODevice::ReadOnly)) {
        check(false, QStringLiteral("session file can be read back"));
        return 1;
    }
    const QByteArray body = raw.readAll();
    raw.close();
    check(body.contains("cairn.session"), QStringLiteral("file is self-identifying"));
    check(!body.contains("%PDF") && body.size() < 512 * 1024,
          QStringLiteral("no file contents are embedded"));

    out() << "\n== map layout ==\n";
    LayoutView view;
    view.resize(760, 470);
    view.setSession(loaded);
    const QList<QString> visible = view.visiblePaths();
    check(visible.size() == loaded.entries.size(),
          QStringLiteral("every entry gets a tile"));

    // Regression: a live capture omits the grid cell from the file, so the view
    // must derive it from the pixel position. When it did not, every tile landed
    // on the same spot and only one was visible after a reload.
    const QHash<QString, QRectF> rects = view.tileRects();
    check(rects.size() == loaded.entries.size(),
          QStringLiteral("the view reports a rectangle per tile"));
    int distinctTiles = 0;
    {
        QSet<QString> seen;
        for (auto it = rects.constBegin(); it != rects.constEnd(); ++it) {
            seen.insert(QStringLiteral("%1,%2")
                            .arg(qRound(it.value().x()))
                            .arg(qRound(it.value().y())));
        }
        distinctTiles = seen.size();
        check(distinctTiles == rects.size(),
              QStringLiteral("no two tiles overlap after a reload (%1 distinct)")
                  .arg(distinctTiles));
    }
    out() << "  info  distinct rendered positions: " << distinctTiles << "\n";

    view.setSearch(QStringLiteral("zzz-no-match"));
    check(view.visiblePaths().isEmpty(), QStringLiteral("search filters the map"));
    view.setSearch(QString());

    QHash<int, bool> onlyArchives;
    onlyArchives.insert(int(Kind::Archive), true);
    for (int kind : {int(Kind::Folder), int(Kind::Executable), int(Kind::Document),
                     int(Kind::Image), int(Kind::Code), int(Kind::Shortcut),
                     int(Kind::Other)}) {
        onlyArchives.insert(kind, false);
    }
    view.setKindFilter(onlyArchives);
    int archiveTiles = view.visiblePaths().size();
    out() << "  info  archive-only tiles: " << archiveTiles << "\n";
    check(archiveTiles == loaded.countOf(Kind::Archive),
          QStringLiteral("kind filter shows only the requested kind"));
    view.setKindFilter({});

    out() << "\n== open by handle ==\n";
    const QString byId = SessionStore::pathForId(loaded.id);
    check(byId == path, QStringLiteral("a bare handle resolves to the same file"));

    QFile::remove(path);
    out() << "\ncleanup   : removed the test session\n";
    out() << (failures == 0 ? "\nALL CHECKS PASSED\n"
                            : QStringLiteral("\n%1 CHECK(S) FAILED\n").arg(failures));
    return failures == 0 ? 0 : 1;
}
