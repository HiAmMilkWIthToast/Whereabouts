// Regression checks for the kind-filter chips. Development helper only.
#include <QApplication>
#include <QPushButton>
#include <QTextStream>
#include <QVBoxLayout>

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

static Entry entry(const QString &name, Kind kind, const QString &suffix)
{
    Entry e;
    e.name = name;
    e.relativePath = name;
    e.kind = kind;
    e.suffix = suffix;
    e.cell = QPoint(0, 0);
    return e;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CairnFilterTest"));
    app.setOrganizationName(QStringLiteral("CairnFilterTest"));

    Session s;
    s.id = QStringLiteral("filtertest");
    s.name = QStringLiteral("Filter test");
    s.rootPath = QStringLiteral("C:/tmp");
    s.capturedAt = QDateTime::currentDateTime();
    s.cellSize = QSize(75, 98);
    s.entries << entry(QStringLiteral("a.txt"), Kind::Document, QStringLiteral("txt"));
    s.entries << entry(QStringLiteral("b.txt"), Kind::Document, QStringLiteral("txt"));
    s.entries << entry(QStringLiteral("c.zip"), Kind::Archive, QStringLiteral("zip"));
    s.entries << entry(QStringLiteral("d.exe"), Kind::Executable, QStringLiteral("exe"));

    MainWindow window;
    window.presentSession(s);

    // Find the chip row and its buttons.
    const QList<QPushButton *> chips =
        window.findChildren<QPushButton *>(QStringLiteral("filterChip"));
    out << "chips found: " << chips.size() << "\n";
    for (QPushButton *c : chips)
        out << "    chip: [" << c->text() << "]\n";
    check(chips.size() == 3, QStringLiteral("one chip per present kind (3 kinds)"));
    // Order follows the inspector's kind order, so assert on content by name
    // rather than on position: the chips are Archive, Executable, Document here.
    bool documentChipOk = false;
    for (QPushButton *c : chips) {
        if (c->text() == QStringLiteral("Document 2"))
            documentChipOk = true;
    }
    check(documentChipOk, QStringLiteral("chip shows the kind and its count"));

    LayoutView *view = window.findChild<LayoutView *>();
    check(view != nullptr, QStringLiteral("layout view is reachable"));
    if (!view)
        return 1;

    check(view->visiblePaths().size() == 4,
          QStringLiteral("all four items visible before filtering (%1)")
              .arg(view->visiblePaths().size()));

    // Switch off Archives.
    for (QPushButton *chip : chips) {
        if (chip->text().startsWith(QStringLiteral("Archive")))
            chip->setChecked(false);
    }
    check(view->visiblePaths().size() == 3,
          QStringLiteral("hiding Archive leaves 3 items (%1)")
              .arg(view->visiblePaths().size()));

    // Switch off everything.
    for (QPushButton *chip : chips)
        chip->setChecked(false);
    check(view->visiblePaths().isEmpty(),
          QStringLiteral("hiding every kind leaves nothing (%1)")
              .arg(view->visiblePaths().size()));

    // Back on.
    for (QPushButton *chip : chips)
        chip->setChecked(true);
    check(view->visiblePaths().size() == 4,
          QStringLiteral("re-enabling every chip restores all items (%1)")
              .arg(view->visiblePaths().size()));

    // A session with a single kind must not show a useless chip row.
    Session one;
    one.id = QStringLiteral("onekind");
    one.name = QStringLiteral("One");
    one.rootPath = QStringLiteral("C:/tmp");
    one.capturedAt = QDateTime::currentDateTime();
    one.cellSize = QSize(75, 98);
    one.entries << entry(QStringLiteral("x.txt"), Kind::Document, QStringLiteral("txt"));
    window.presentSession(one);

    const QList<QPushButton *> afterOne =
        window.findChildren<QPushButton *>(QStringLiteral("filterChip"));
    check(afterOne.isEmpty(),
          QStringLiteral("a single-kind session shows no chip row (got %1)")
              .arg(afterOne.size()));

    out << "\n" << (failures == 0 ? "FILTER PASSED\n" : "FILTER FAILED\n");
    out.flush();
    return failures == 0 ? 0 : 1;
}
