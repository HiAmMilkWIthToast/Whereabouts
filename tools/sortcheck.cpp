// Regression checks for list sorting. Development helper only.
#include <QApplication>
#include <QComboBox>
#include <QPushButton>
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

static Entry makeEntry(const QString &name, Kind kind, qint64 size, int dayOffset,
                       const QString &suffix)
{
    Entry e;
    e.name = name;
    e.relativePath = name;
    e.kind = kind;
    e.suffix = suffix;
    e.size = size;
    e.modified = QDateTime(QDate(2026, 1, 1), QTime(12, 0)).addDays(dayOffset);
    e.cell = QPoint(0, 0);
    return e;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CairnSortTest"));
    app.setOrganizationName(QStringLiteral("CairnSortTest"));

    Session s;
    s.id = QStringLiteral("sorttest");
    s.name = QStringLiteral("Sort");
    s.rootPath = QStringLiteral("C:/tmp");
    s.capturedAt = QDateTime::currentDateTime();
    s.cellSize = QSize(75, 98);
    s.entries << makeEntry(QStringLiteral("middle.txt"), Kind::Document, 300, 2,
                           QStringLiteral("txt"));
    s.entries << makeEntry(QStringLiteral("alpha.txt"), Kind::Document, 100, 3,
                           QStringLiteral("txt"));
    s.entries << makeEntry(QStringLiteral("zulu.zip"), Kind::Archive, 900, 1,
                           QStringLiteral("zip"));
    s.entries << makeEntry(QStringLiteral("folder_a"), Kind::Folder, 0, 0,
                           QStringLiteral("dir"));

    LayoutView view;
    view.resize(800, 600);
    view.setSession(s);
    view.setMode(LayoutView::Mode::List);

    const auto names = [&]() { return view.visiblePaths(); };
    const auto joined = [&]() { return names().join(QStringLiteral(", ")); };

    view.setSort(LayoutView::Sort::Name, false);
    out << "  name asc: " << joined() << "\n";
    check(names() == QStringList{QStringLiteral("folder_a"), QStringLiteral("alpha.txt"),
                                 QStringLiteral("middle.txt"), QStringLiteral("zulu.zip")},
          QStringLiteral("name ascending, folder first"));

    view.setSort(LayoutView::Sort::Name, true);
    out << "  name desc: " << joined() << "\n";
    {
        const QStringList n = names();
        check(n.size() == 4 && n.first() == QStringLiteral("folder_a")
                  && n.at(1) == QStringLiteral("zulu.zip"),
              QStringLiteral("name descending keeps the folder first"));
    }

    view.setSort(LayoutView::Sort::Size, false);
    out << "  size asc: " << joined() << "\n";
    {
        const QStringList n = names();
        check(n.size() == 4 && n.first() == QStringLiteral("folder_a")
                  && n.at(1) == QStringLiteral("alpha.txt")
                  && n.at(3) == QStringLiteral("zulu.zip"),
              QStringLiteral("size ascending, folder first"));
    }

    view.setSort(LayoutView::Sort::Size, true);
    out << "  size desc: " << joined() << "\n";
    {
        const QStringList n = names();
        check(n.size() == 4 && n.first() == QStringLiteral("folder_a")
                  && n.at(1) == QStringLiteral("zulu.zip")
                  && n.at(3) == QStringLiteral("alpha.txt"),
              QStringLiteral("size descending, folder still first"));
    }

    view.setSort(LayoutView::Sort::Modified, false);
    out << "  modified asc: " << joined() << "\n";
    {
        const QStringList n = names();
        check(n.size() == 4 && n.at(1) == QStringLiteral("zulu.zip")
                  && n.at(3) == QStringLiteral("alpha.txt"),
              QStringLiteral("modified ascending"));
    }

    // Sorting must survive a mode toggle rather than silently resetting.
    view.setSort(LayoutView::Sort::Size, true);
    view.setMode(LayoutView::Mode::Map);
    view.setMode(LayoutView::Mode::List);
    out << "  after map round trip: " << joined() << "\n";
    {
        const QStringList n = names();
        check(n.size() == 4 && n.at(1) == QStringLiteral("zulu.zip"),
              QStringLiteral("sort survives a map/list round trip"));
    }

    // ---- the sort control in the real window
    {
        out << "\n== sort control in the window ==\n";
        MainWindow window;
        window.presentSession(s);

        auto *box = window.findChild<QComboBox *>(QStringLiteral("field"));
        check(box != nullptr, QStringLiteral("sort control exists"));
        if (box) {
            check(box->count() == 4, QStringLiteral("four sort options offered"));

            LayoutView *v = window.findChild<LayoutView *>();
            auto *toggle = window.findChild<QPushButton *>(QStringLiteral("secondary"));
            check(v != nullptr && toggle != nullptr,
                  QStringLiteral("view and toggle are reachable"));

            // The sort bar must only appear in list mode.
            if (v && toggle) {
                // Find the bar: the parent widget of the sort box.
                QWidget *bar = box->parentWidget();
                check(bar && bar->isHidden(),
                      QStringLiteral("sort bar is hidden in map mode"));

                toggle->click();   // switch to list
                check(v->mode() == LayoutView::Mode::List,
                      QStringLiteral("toggle switches to list mode"));
                check(bar && !bar->isHidden(),
                      QStringLiteral("sort bar appears in list mode"));

                toggle->click();   // back to map
                check(v->mode() == LayoutView::Mode::Map,
                      QStringLiteral("toggle switches back to map mode"));
                check(bar && bar->isHidden(),
                      QStringLiteral("sort bar hides again in map mode"));
            }
        }
    }

    out << "\n" << (failures == 0 ? "SORT PASSED\n" : "SORT FAILED\n");
    out.flush();
    return failures == 0 ? 0 : 1;
}
