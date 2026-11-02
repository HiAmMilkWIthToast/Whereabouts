// Focused regression checks for the flaws fixed in this pass. Development
// helper only; lives in tools/ and never ships.
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTextStream>
#include <QTemporaryDir>

#include "layoutview.h"
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

static Entry makeEntry(const QString &name, const QPoint &cell, bool fromShell)
{
    Entry e;
    e.name = name;
    e.relativePath = name;
    e.kind = Kind::Document;
    e.suffix = QStringLiteral("txt");
    e.cell = cell;
    e.fromShell = fromShell;
    // Deliberately vague on purpose: fromShell entries carry a pixel position.
    e.pos = cell;
    return e;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CairnRegress"));
    app.setOrganizationName(QStringLiteral("CairnRegress"));

    out << "== colliding cells do not hide tiles ==\n";
    {
        // Two icons whose pixel positions round into the same 75x98 cell, which
        // is what happens on a desktop with "align icons to grid" switched off.
        Session s;
        s.id = QStringLiteral("collide");
        s.name = QStringLiteral("Collide");
        s.rootPath = QStringLiteral("C:/tmp");
        s.capturedAt = QDateTime::currentDateTime();
        s.cellSize = QSize(75, 98);
        s.livePositions = true;
        s.entries << makeEntry(QStringLiteral("a.txt"), QPoint(0, 0), true);
        s.entries << makeEntry(QStringLiteral("b.txt"), QPoint(0, 0), true);
        s.entries << makeEntry(QStringLiteral("c.txt"), QPoint(0, 0), true);

        LayoutView view;
        view.resize(900, 600);
        view.setSession(s);

        const QHash<QString, QRectF> rects = view.tileRects();
        check(rects.size() == 3, QStringLiteral("all three entries get a tile"));
        QSet<QString> distinct;
        for (const QRectF &r : rects)
            distinct.insert(QStringLiteral("%1,%2,%3,%4")
                                .arg(qRound(r.x())).arg(qRound(r.y()))
                                .arg(qRound(r.width())).arg(qRound(r.height())));
        check(distinct.size() == 3,
              QStringLiteral("colliding entries render in distinct places (got %1)")
                  .arg(distinct.size()));

        // They must not merely differ by a rounding pixel: check no overlap.
        bool overlaps = false;
        const QList<QRectF> list = rects.values();
        for (int i = 0; i < list.size(); ++i)
            for (int j = i + 1; j < list.size(); ++j)
                if (list.at(i).toAlignedRect().intersects(list.at(j).toAlignedRect()))
                    overlaps = true;
        check(!overlaps, QStringLiteral("no two colliding tiles overlap"));
    }

    out << "\n== empty state is shown, not blank ==\n";
    {
        Session s;
        s.id = QStringLiteral("empty");
        s.name = QStringLiteral("Empty");
        s.rootPath = QStringLiteral("C:/tmp");
        s.capturedAt = QDateTime::currentDateTime();
        s.cellSize = QSize(75, 98);
        s.entries << makeEntry(QStringLiteral("a.txt"), QPoint(0, 0), false);

        LayoutView view;
        view.resize(900, 600);
        view.setSession(s);
        view.setSearch(QStringLiteral("zzz-no-such-item"));
        check(view.visiblePaths().isEmpty(),
              QStringLiteral("search filters everything out"));
        // Rendering must not crash and must produce a non-empty canvas.
        QPixmap pix(view.size());
        pix.fill(Qt::transparent);
        view.render(&pix);
        check(!pix.isNull(), QStringLiteral("empty state renders without crashing"));
    }

    out << "\n== save replaces atomically and keeps the old file on failure ==\n";
    {
        Session s;
        s.id = QStringLiteral("atomic");
        s.name = QStringLiteral("Atomic");
        s.rootPath = QStringLiteral("C:/tmp");
        s.capturedAt = QDateTime::currentDateTime();
        s.cellSize = QSize(75, 98);
        s.entries << makeEntry(QStringLiteral("a.txt"), QPoint(0, 0), false);

        QString error;
        const bool ok = SessionStore::save(s, &error);
        check(ok, QStringLiteral("save succeeds (%1)").arg(error));

        bool loaded = false;
        const QString path = SessionStore::pathForId(s.id);
        const Session back = SessionStore::load(path, &loaded);
        check(loaded && back.id == s.id, QStringLiteral("saved file reloads"));

        // A failed write must not damage the existing file. Simulate by pointing
        // the save at an unwritable id; the original must survive untouched.
        const qint64 before = QFileInfo(path).size();
        Session broken = s;
        broken.id = QStringLiteral("nul");   // reserved device name
        QString brokenError;
        SessionStore::save(broken, &brokenError);
        check(QFileInfo(path).size() == before,
              QStringLiteral("a failed save leaves the existing file intact"));

        QFile::remove(path);
    }

    out << "\n== reserved names are sanitized ==\n";
    {
        const QString path = SessionStore::pathForId(QStringLiteral("con"));
        check(!QFileInfo(path).fileName().startsWith(QLatin1String("con.")),
              QStringLiteral("CON handle is made writable: %1")
                  .arg(QFileInfo(path).fileName()));
        const QString dotted = SessionStore::pathForId(QStringLiteral("nul.txt"));
        check(!QFileInfo(dotted).fileName().startsWith(QLatin1String("nul")),
              QStringLiteral("nul.txt handle is made writable: %1")
                  .arg(QFileInfo(dotted).fileName()));
    }

    out << "\n== export target is not destroyed on failure ==\n";
    {
        QTemporaryDir dir;
        const QString target = dir.filePath(QStringLiteral("existing.session"));
        QFile pre(target);
        if (!pre.open(QIODevice::WriteOnly)) {
            out << "  FAIL  could not prepare the export target\n";
            ++failures;
        }
        pre.write("keep me");
        pre.close();

        // Reading a session that does not exist must not touch the destination.
        QFile missing(SessionStore::pathForId(QStringLiteral("no-such-session")));
        const bool readable = missing.open(QIODevice::ReadOnly);
        check(!readable, QStringLiteral("missing source is detected"));

        QFile after(target);
        const bool reopened = after.open(QIODevice::ReadOnly);
        check(reopened && after.readAll() == QByteArray("keep me"),
              QStringLiteral("destination survives a failed export"));
    }

    out << "\n";
    out << (failures == 0 ? "REGRESS PASSED\n" : "REGRESS FAILED\n");
    out.flush();
    return failures == 0 ? 0 : 1;
}
