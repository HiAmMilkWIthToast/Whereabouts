// Regression checks for keyboard navigation. Development helper only.
#include <QApplication>
#include <QKeyEvent>
#include <QTextStream>

#include "layoutview.h"
#include "sessionmodel.h"

static QTextStream out(stdout);
static int failures = 0;

static void check(bool ok, const QString &what)
{
    out << (ok ? "  PASS  " : "  FAIL  ") << what << "\n";
    if (!ok)
        ++failures;
}

static Entry makeEntry(const QString &name, const QPoint &cell)
{
    Entry e;
    e.name = name;
    e.relativePath = name;
    e.kind = Kind::Document;
    e.suffix = QStringLiteral("txt");
    e.cell = cell;
    e.pos = QPoint(cell.x() * 75, cell.y() * 98);
    return e;
}

static void press(QWidget *w, int key)
{
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
    QApplication::sendEvent(w, &event);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CairnKeyTest"));
    app.setOrganizationName(QStringLiteral("CairnKeyTest"));

    // A 2x2 grid so up/down/left/right each have an unambiguous target.
    Session s;
    s.id = QStringLiteral("keytest");
    s.name = QStringLiteral("Keys");
    s.rootPath = QStringLiteral("C:/tmp");
    s.capturedAt = QDateTime::currentDateTime();
    s.cellSize = QSize(75, 98);
    s.entries << makeEntry(QStringLiteral("a.txt"), QPoint(0, 0));
    s.entries << makeEntry(QStringLiteral("b.txt"), QPoint(0, 1));
    s.entries << makeEntry(QStringLiteral("c.txt"), QPoint(1, 0));
    s.entries << makeEntry(QStringLiteral("d.txt"), QPoint(1, 1));

    LayoutView view;
    view.resize(800, 600);
    view.setSession(s);
    view.show();

    check(view.focusPolicy() == Qt::StrongFocus,
          QStringLiteral("view accepts keyboard focus"));

    // Nothing selected yet; the first arrow press selects the first tile.
    press(&view, Qt::Key_Right);
    const QString first = view.selectedPath();
    check(!first.isEmpty(),
          QStringLiteral("first arrow press selects something (%1)").arg(first));

    // Walk right from the first tile and confirm the selection actually changes.
    press(&view, Qt::Key_Right);
    const QString second = view.selectedPath();
    check(!second.isEmpty() && second != first,
          QStringLiteral("Right moves to a different tile (%1 -> %2)")
              .arg(first, second));

    press(&view, Qt::Key_Down);
    const QString third = view.selectedPath();
    check(!third.isEmpty() && third != second,
          QStringLiteral("Down moves to a different tile (%1 -> %2)")
              .arg(second, third));

    // Escape clears it.
    press(&view, Qt::Key_Escape);
    check(view.selectedPath().isEmpty(),
          QStringLiteral("Escape clears the selection"));

    // Enter activates the current selection.
    bool activated = false;
    QString activatedPath;
    QObject::connect(&view, &LayoutView::entryActivated,
                     [&](const QString &p) { activated = true; activatedPath = p; });
    press(&view, Qt::Key_Left);
    const QString beforeEnter = view.selectedPath();
    press(&view, Qt::Key_Return);
    check(activated && activatedPath == beforeEnter,
          QStringLiteral("Enter activates the selected item"));

    // List mode: up/down walk the column and wrap at the ends.
    view.setMode(LayoutView::Mode::List);
    press(&view, Qt::Key_Escape);
    press(&view, Qt::Key_Down);
    check(!view.selectedPath().isEmpty(),
          QStringLiteral("list mode selects on the first Down"));
    const QString listFirst = view.selectedPath();
    press(&view, Qt::Key_Up);
    const QString listLast = view.selectedPath();
    check(!listLast.isEmpty() && listLast != listFirst,
          QStringLiteral("list mode wraps upward past the first row"));

    // An empty view must not crash or invent a selection.
    Session empty;
    empty.id = QStringLiteral("empty");
    empty.rootPath = QStringLiteral("C:/tmp");
    empty.capturedAt = QDateTime::currentDateTime();
    empty.cellSize = QSize(75, 98);
    view.setSession(empty);
    press(&view, Qt::Key_Right);
    check(view.selectedPath().isEmpty(),
          QStringLiteral("an empty session cannot select anything"));

    out << "\n" << (failures == 0 ? "KEYS PASSED\n" : "KEYS FAILED\n");
    out.flush();
    return failures == 0 ? 0 : 1;
}
