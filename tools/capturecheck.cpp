
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSettings>
#include <QTextStream>

#include "sessionstore.h"
#include "theme.h"

static QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

static void dumpSession(const Session &session)
{
    out() << "id          : " << session.id << "\n";
    out() << "name        : " << session.name << "\n";
    out() << "root        : " << session.rootPath << "\n";
    out() << "live        : " << (session.livePositions ? "yes" : "no") << "\n";
    out() << "screen      : " << session.screen.width() << "x"
          << session.screen.height() << "\n";
    out() << "cell        : " << session.cellSize.width() << "x"
          << session.cellSize.height() << "\n";
    out() << "summary     : " << session.summary() << "\n";
    out() << "entries     : " << session.entries.size() << "\n";
    for (const Entry &entry : session.entries) {
        out() << QStringLiteral("  %1  kind=%2  size=%3  pos=(%4,%5) cell=(%6,%7) live=%8")
                     .arg(entry.name, -26)
                     .arg(kindShortName(entry.kind), -4)
                     .arg(entry.size)
                     .arg(entry.pos.x()).arg(entry.pos.y())
                     .arg(entry.cell.x()).arg(entry.cell.y())
                     .arg(entry.fromShell ? "y" : "n")
              << "\n";
        if (!entry.target.isEmpty())
            out() << "      target: " << entry.target << "\n";
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CairnCheck"));
    app.setOrganizationName(QStringLiteral("CairnCheck"));

    bool failed = false;

    out() << "== desktop ==\n";
    const QString desktop = SessionStore::desktopPath();
    out() << "desktop path: " << desktop << "\n";
    out() << "isDesktop   : " << SessionStore::isDesktopPath(desktop) << "\n";

    Session desktopSession = SessionStore::capture(desktop);
    dumpSession(desktopSession);

    QString error;
    const bool saved = SessionStore::save(desktopSession, &error);
    out() << "save        : " << (saved ? "ok" : "FAILED " + error) << "\n";
    const QString path = SessionStore::pathForId(desktopSession.id);
    out() << "file        : " << path << "\n";
    out() << "bytes       : " << QFileInfo(path).size() << "\n";

    bool ok = false;
    const Session reloaded = SessionStore::load(path, &ok);
    out() << "reload      : " << (ok ? "ok" : "FAILED")
          << " entries=" << reloaded.entries.size() << "\n";
    out() << "roundtrip   : "
          << (reloaded.entries.size() == desktopSession.entries.size()
                  && reloaded.id == desktopSession.id
                  && reloaded.livePositions == desktopSession.livePositions
              ? "match" : "MISMATCH") << "\n";

    out() << "\n== plain folder ==\n";

    const QString folder = argc > 1
        ? QString::fromLocal8Bit(argv[1])
        : QStringLiteral(CAIRN_SAMPLE_DIR);
    out() << "folder      : " << folder << "\n";
    if (!QFileInfo(folder).isDir()) {
        out() << "  FAIL: sample folder does not exist\n";
        failed = true;
    }
    Session folderSession = SessionStore::capture(folder);
    dumpSession(folderSession);
    if (folderSession.entries.isEmpty()) {
        out() << "  FAIL: captured no entries from the sample folder\n";
        failed = true;
    }

    out() << "\n== library ==\n";
    const QList<Session> all = SessionStore::listAll();
    for (const Session &session : all)
        out() << "  " << session.id << "  " << session.entries.size() << " items\n";

    QFile::remove(path);
    out() << "\ncleanup     : removed " << path << "\n";

    if (!saved || !ok)
        failed = true;
    out() << (failed ? "\nCHECK FAILED\n" : "\nCHECK PASSED\n");
    out().flush();
    return failed ? 1 : 0;
}
