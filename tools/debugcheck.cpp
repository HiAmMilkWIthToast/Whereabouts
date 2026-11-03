
#include <QApplication>
#include <QLineEdit>
#include <QPushButton>
#include <QTextStream>

#include "filterchip.h"
#include "inspector.h"
#include "layoutview.h"
#include "mainwindow.h"
#include "sessionmodel.h"
#include <QTime>
#include <QLabel>
#include <QDate>
#include "systeminfo.h"

static QTextStream out(stdout);
static int failures = 0;

static void check(bool ok, const QString &what)
{
    out << (ok ? "  PASS  " : "  FAIL  ") << what << "\n";
    if (!ok)
        ++failures;
}

static Entry ent(const QString &name, Kind kind, const QString &suffix,
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

static Session sampleSession()
{
    Session s;
    s.id = QStringLiteral("debug-pass");
    s.name = QStringLiteral("Debug pass");
    s.rootPath = QStringLiteral("C:/tmp");
    s.capturedAt = QDateTime::currentDateTime();
    s.cellSize = QSize(75, 98);
    s.entries << ent(QStringLiteral("alpha.txt"), Kind::Document, QStringLiteral("txt"), 0, 0);
    s.entries << ent(QStringLiteral("beta.zip"), Kind::Archive, QStringLiteral("zip"), 1, 0);
    s.entries << ent(QStringLiteral("gamma.png"), Kind::Image, QStringLiteral("png"), 2, 0);
    return s;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CairnDebugTest"));
    app.setOrganizationName(QStringLiteral("CairnDebugTest"));

    {
        out << "== Escape clears the inspector ==\n";

        MainWindow window;
        window.resize(1000, 640);
        window.presentSession(sampleSession());

        auto *view = window.findChild<LayoutView *>();
        auto *insp = window.findChild<Inspector *>();
        check(view && insp, QStringLiteral("view and inspector exist"));
        if (!view || !insp)
            return 1;

        view->moveSelection(0, 1);
        const QString picked = view->selectedPath();
        check(!picked.isEmpty(), QStringLiteral("an item is selected (%1)").arg(picked));
        check(insp->hasEntry(), QStringLiteral("inspector shows that entry"));

        view->clearSelection();
        check(!insp->hasEntry(),
              QStringLiteral("inspector falls back to the summary after Escape"));
    }

    {
        out << "\n== a search that hides the selection drops it ==\n";

        LayoutView view;
        view.setSession(sampleSession());

        view.moveSelection(0, 1);
        view.moveSelection(0, 1);
        check(view.selectedPath() == QStringLiteral("beta.zip"),
              QStringLiteral("beta.zip is selected (%1)").arg(view.selectedPath()));

        QString emitted;
        int emissions = 0;
        QObject::connect(&view, &LayoutView::entrySelected,
                         [&](const QString &p) { emitted = p; ++emissions; });

        view.setSearch(QStringLiteral("alpha"));
        check(view.selectedPath().isEmpty(),
              QStringLiteral("selection drops when search hides it (%1)")
                  .arg(view.selectedPath()));
        check(emissions == 1 && emitted.isEmpty(),
              QStringLiteral("and it reports an empty selection"));

        view.setSearch(QString());
        check(view.selectedPath().isEmpty(),
              QStringLiteral("selection stays clear after clearing the search"));
    }

    {
        out << "\n== a live refresh keeps the user's zoom ==\n";

        LayoutView view;
        view.resize(800, 600);
        view.setSession(sampleSession());
        const qreal initial = view.zoom();

        view.setZoom(2.0);
        check(qFuzzyCompare(view.zoom(), 2.0),
              QStringLiteral("zoom applied (%1)").arg(view.zoom()));

        Session refreshed = sampleSession();
        refreshed.entries << ent(QStringLiteral("delta.md"), Kind::Document,
                                 QStringLiteral("md"), 3, 0);
        view.setSession(refreshed, false);

        check(qFuzzyCompare(view.zoom(), 2.0),
              QStringLiteral("zoom survives the refresh (%1)").arg(view.zoom()));
        check(view.visiblePaths().size() == 4,
              QStringLiteral("the new item is shown (%1)").arg(view.visiblePaths().size()));

        view.setSession(sampleSession());
        check(!qFuzzyCompare(view.zoom(), 2.0),
              QStringLiteral("opening a session still refits (%1)").arg(view.zoom()));
        Q_UNUSED(initial)
    }

    {
        out << "\n== the chip row does not accumulate ==\n";

        MainWindow window;
        window.presentSession(sampleSession());
        const int first = window.findChildren<FilterChip *>().size();
        check(first == 3, QStringLiteral("three kinds make three chips (%1)").arg(first));

        Session one;
        one.id = QStringLiteral("one");
        one.name = QStringLiteral("One");
        one.rootPath = QStringLiteral("C:/tmp");
        one.capturedAt = QDateTime::currentDateTime();
        one.cellSize = QSize(75, 98);
        one.entries << ent(QStringLiteral("x.txt"), Kind::Document, QStringLiteral("txt"), 0, 0);
        window.presentSession(one);
        check(window.findChildren<FilterChip *>().isEmpty(),
              QStringLiteral("one kind shows no chips"));

        window.presentSession(sampleSession());
        const int again = window.findChildren<FilterChip *>().size();
        check(again == 3,
              QStringLiteral("back to three chips, no leftovers (%1)").arg(again));
    }

    {
        out << "\n== chip hover repaints without changing state ==\n";
        FilterChip chip(QStringLiteral("Folder"), 3);
        chip.resize(chip.sizeHint());
        const bool before = chip.isChecked();

        QEvent enter(QEvent::Enter);
        QApplication::sendEvent(&chip, &enter);
        check(chip.isChecked() == before,
              QStringLiteral("hover does not toggle the chip"));

        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(&chip, &leave);
        check(chip.isChecked() == before,
              QStringLiteral("leaving does not toggle the chip"));
    }

    {
        out << "\n== the greeting and the clock ==\n";

        check(SystemInfo::greetingFor(QTime(8, 30)) == QStringLiteral("Good morning"),
              QStringLiteral("morning greeting (%1)")
                  .arg(SystemInfo::greetingFor(QTime(8, 30))));
        check(SystemInfo::greetingFor(QTime(13, 0)) == QStringLiteral("Good afternoon"),
              QStringLiteral("afternoon greeting (%1)")
                  .arg(SystemInfo::greetingFor(QTime(13, 0))));
        check(SystemInfo::greetingFor(QTime(21, 15)) == QStringLiteral("Good evening"),
              QStringLiteral("evening greeting (%1)")
                  .arg(SystemInfo::greetingFor(QTime(21, 15))));

        check(!SystemInfo::greetingFor(QTime()).isEmpty(),
              QStringLiteral("an invalid time still yields a greeting"));

        const QString who = SystemInfo::userName();
        check(!who.isEmpty(), QStringLiteral("the user name is detectable (%1)").arg(who));
        const QByteArray env = qgetenv("USERNAME");
        if (!env.isEmpty()) {
            check(who.compare(QString::fromLocal8Bit(env), Qt::CaseInsensitive) == 0,
                  QStringLiteral("the name matches the account (%1 vs %2)")
                      .arg(who, QString::fromLocal8Bit(env)));
        }

        check(who.isEmpty() || who.size() >= 2,
              QStringLiteral("the name is not a fragment (len %1)").arg(who.size()));

        MainWindow window;
        window.presentSession(sampleSession());
        auto *welcome = window.findChild<QLabel *>(QStringLiteral("welcomeLine"));
        check(welcome != nullptr, QStringLiteral("the Sessions page shows a greeting"));
        if (welcome) {
            check(welcome->text().contains(SystemInfo::greetingFor(QTime::currentTime())),
                  QStringLiteral("it uses the time of day (%1)").arg(welcome->text()));
            check(who.isEmpty() || welcome->text().contains(who),
                  QStringLiteral("it names the user (%1)").arg(welcome->text()));
        }

        auto *status = window.findChild<QWidget *>(QStringLiteral("statusBar"));
        check(status != nullptr, QStringLiteral("the footer exists"));
        if (status) {
            bool clockShown = false;
            bool dateShown = false;
            for (QLabel *l : status->findChildren<QLabel *>()) {

                if (l->text().count(QLatin1Char(':')) == 2)
                    clockShown = true;

                if (l->text().contains(QString::number(QDate::currentDate().year())))
                    dateShown = true;
            }
            check(clockShown, QStringLiteral("the footer shows a clock"));
            check(dateShown, QStringLiteral("the footer shows the date"));
        }
    }

    out << "\n" << (failures == 0 ? "DEBUG PASSED\n" : "DEBUG FAILED\n");
    out.flush();
    return failures == 0 ? 0 : 1;
}
