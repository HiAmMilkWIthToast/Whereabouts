// Regression checks for window geometry persistence. Development helper only.
#include <QApplication>
#include <QSettings>
#include <QTextStream>

#include "mainwindow.h"

static QTextStream out(stdout);
static int failures = 0;

static void check(bool ok, const QString &what)
{
    out << (ok ? "  PASS  " : "  FAIL  ") << what << "\n";
    if (!ok)
        ++failures;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CairnGeomTest"));
    app.setOrganizationName(QStringLiteral("CairnGeomTest"));

    // Start from a clean slate.
    QSettings().remove(QStringLiteral("window/libraryGeometry"));
    QSettings().sync();

    {
        MainWindow w;
        w.resize(900, 700);
        w.move(120, 90);
        w.show();
        QApplication::processEvents();

        // closeEvent writes the geometry.
        w.close();
        QApplication::processEvents();
    }

    QSettings().sync();
    const QByteArray saved =
        QSettings().value(QStringLiteral("window/libraryGeometry")).toByteArray();
    check(!saved.isEmpty(), QStringLiteral("geometry is written on close"));

    {
        MainWindow w2;
        w2.show();
        QApplication::processEvents();
        // A restored geometry comes back as the frame size; allow a few pixels
        // for the frameless margins the window applies on show.
        const QSize size = w2.size();
        out << "  restored size: " << size.width() << "x" << size.height() << "\n";
        check(qAbs(size.width() - 900) <= 16 && qAbs(size.height() - 700) <= 16,
              QStringLiteral("size is restored (%1x%2)").arg(size.width()).arg(size.height()));
        w2.close();
    }

    // Corrupt the stored value: the window must still open somewhere sane
    // instead of failing to appear.
    {
        QSettings().setValue(QStringLiteral("window/libraryGeometry"),
                             QByteArray("not-a-valid-geometry-blob"));
        QSettings().sync();

        MainWindow w3;
        w3.show();
        QApplication::processEvents();
        check(w3.isVisible(), QStringLiteral("a corrupt geometry still opens a visible window"));
        check(w3.width() >= 560 && w3.height() >= 420,
              QStringLiteral("window keeps its minimum usable size"));
        w3.close();
    }

    QSettings().remove(QStringLiteral("window/libraryGeometry"));
    QSettings().sync();

    out << "\n" << (failures == 0 ? "GEOMETRY PASSED\n" : "GEOMETRY FAILED\n");
    out.flush();
    return failures == 0 ? 0 : 1;
}
