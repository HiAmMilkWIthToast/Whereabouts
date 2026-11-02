// Regression checks for the bugs fixed in this pass. Development helper only.
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include "filterchip.h"
#include <QTextStream>

#include "layoutview.h"
#include "mainwindow.h"
#include "sessionmodel.h"

static QTextStream out(stdout);
static int failures = 0;

static void check(bool ok, const QString &what)
{
    out << (ok ? "  PASS  " : "  FAIL  ") << what << "\n";
    if (!ok)
        ++failures;
}

static Entry makeEntry(const QString &name, Kind kind, const QString &suffix,
                       int col, int row)
{
    Entry e;
    e.name = name;
    e.relativePath = name;
    e.kind = kind;
    e.suffix = suffix;
    e.cell = QPoint(col, row);
    e.pos = QPoint(col * 75, row * 98);
    return e;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CairnAuditTest"));
    app.setOrganizationName(QStringLiteral("CairnAuditTest"));

    // ---------------------------------------------------------------- bug 1/2
    // Hiding the kind of the selected item must clear the selection rather than
    // leaving the inspector describing something that is no longer drawn.
    {
        out << "== selection is dropped when its kind is filtered out ==\n";

        Session s;
        s.id = QStringLiteral("audit");
        s.name = QStringLiteral("Audit");
        s.rootPath = QStringLiteral("C:/tmp");
        s.capturedAt = QDateTime::currentDateTime();
        s.cellSize = QSize(75, 98);
        s.entries << makeEntry(QStringLiteral("a.txt"), Kind::Document,
                               QStringLiteral("txt"), 0, 0);
        s.entries << makeEntry(QStringLiteral("b.zip"), Kind::Archive,
                               QStringLiteral("zip"), 0, 1);
        s.entries << makeEntry(QStringLiteral("c.png"), Kind::Image,
                               QStringLiteral("png"), 1, 0);

        LayoutView view;
        view.resize(800, 600);
        view.setSession(s);

        // The first arrow press selects the first tile, so press twice to land
        // on the archive that sits below it.
        view.moveSelection(0, 1);
        view.moveSelection(0, 1);
        const QString selected = view.selectedPath();
        check(selected == QStringLiteral("b.zip"),
              QStringLiteral("the archive is selected (%1)").arg(selected));

        // Hide archives. The selection cannot survive.
        QHash<int, bool> filter;
        filter.insert(int(Kind::Archive), false);
        view.setKindFilter(filter);

        check(view.selectedPath().isEmpty(),
              QStringLiteral("selection clears when its kind is hidden (%1)")
                  .arg(view.selectedPath()));

        // Restoring the kind must not resurrect the old selection.
        view.setKindFilter(QHash<int, bool>());
        check(view.selectedPath().isEmpty(),
              QStringLiteral("selection stays cleared after the filter is removed"));
    }

    // ------------------------------------------------- selection signal contract
    // The window relies on entrySelected("") meaning "nothing selected" so the
    // inspector can fall back to the session summary.
    {
        out << "\n== clearing emits an empty path ==\n";
        Session s;
        s.id = QStringLiteral("sig");
        s.rootPath = QStringLiteral("C:/tmp");
        s.capturedAt = QDateTime::currentDateTime();
        s.cellSize = QSize(75, 98);
        s.entries << makeEntry(QStringLiteral("a.txt"), Kind::Document,
                               QStringLiteral("txt"), 0, 0);

        LayoutView view;
        view.setSession(s);
        view.moveSelection(0, 1);

        QString last = QStringLiteral("<none>");
        int emissions = 0;
        QObject::connect(&view, &LayoutView::entrySelected,
                         [&](const QString &p) { last = p; ++emissions; });

        view.clearSelection();
        check(emissions == 1 && last.isEmpty(),
              QStringLiteral("Escape emits one empty selection (%1, %2 emissions)")
                  .arg(last.isEmpty() ? QStringLiteral("empty") : last)
                  .arg(emissions));
    }

    // ---------------------------------------------------------------- bug 4
    // Presenting the same session twice must not leave the window in a state
    // where a third present resets anything, and presenting must be idempotent.
    {
        out << "\n== presenting the same session repeatedly is stable ==\n";

        Session s;
        s.id = QStringLiteral("idem");
        s.name = QStringLiteral("Idem");
        s.rootPath = QStringLiteral("C:/tmp");
        s.capturedAt = QDateTime::currentDateTime();
        s.cellSize = QSize(75, 98);
        s.entries << makeEntry(QStringLiteral("a.txt"), Kind::Document,
                               QStringLiteral("txt"), 0, 0);
        s.entries << makeEntry(QStringLiteral("b.zip"), Kind::Archive,
                               QStringLiteral("zip"), 0, 1);

        MainWindow window;
        window.presentSession(s);

        const int chipsFirst =
            window.findChildren<FilterChip *>(QStringLiteral("filterChip")).size();
        window.presentSession(s);
        const int chipsSecond =
            window.findChildren<FilterChip *>(QStringLiteral("filterChip")).size();

        check(chipsFirst == chipsSecond && chipsFirst == 2,
              QStringLiteral("chip count is stable across re-presenting (%1 then %2)")
                  .arg(chipsFirst)
                  .arg(chipsSecond));
    }

    out << "\n" << (failures == 0 ? "AUDIT PASSED\n" : "AUDIT FAILED\n");
    out.flush();
    return failures == 0 ? 0 : 1;
}
