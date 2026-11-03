
#include <QApplication>
#include <QHash>
#include <QPushButton>
#include <QTextStream>
#include <QVBoxLayout>

#include "filterchip.h"
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

    const QList<FilterChip *> chips =
        window.findChildren<FilterChip *>(QStringLiteral("filterChip"));
    out << "chips found: " << chips.size() << "\n";
    for (FilterChip *c : chips)
        out << "    chip: [" << c->label() << " " << c->count() << "]\n";
    check(chips.size() == 3, QStringLiteral("one chip per present kind (3 kinds)"));

    bool documentChipOk = false;
    for (FilterChip *c : chips) {
        if (c->label() == QStringLiteral("Document") && c->count() == 2)
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

    for (FilterChip *chip : chips) {
        if (chip->label() == QStringLiteral("Archive"))
            chip->setChecked(false);
    }
    check(view->visiblePaths().size() == 3,
          QStringLiteral("hiding Archive leaves 3 items (%1)")
              .arg(view->visiblePaths().size()));

    for (FilterChip *chip : chips)
        chip->setChecked(false);
    check(view->visiblePaths().isEmpty(),
          QStringLiteral("hiding every kind leaves nothing (%1)")
              .arg(view->visiblePaths().size()));

    for (FilterChip *chip : chips)
        chip->setChecked(true);
    check(view->visiblePaths().size() == 4,
          QStringLiteral("re-enabling every chip restores all items (%1)")
              .arg(view->visiblePaths().size()));

    Session one;
    one.id = QStringLiteral("onekind");
    one.name = QStringLiteral("One");
    one.rootPath = QStringLiteral("C:/tmp");
    one.capturedAt = QDateTime::currentDateTime();
    one.cellSize = QSize(75, 98);
    one.entries << entry(QStringLiteral("x.txt"), Kind::Document, QStringLiteral("txt"));
    window.presentSession(one);

    const QList<FilterChip *> afterOne =
        window.findChildren<FilterChip *>(QStringLiteral("filterChip"));
    check(afterOne.isEmpty(),
          QStringLiteral("a single-kind session shows no chip row (got %1)")
              .arg(afterOne.size()));

    {
        MainWindow shot;
        Session two;
        two.id = QStringLiteral("centretest");
        two.name = QStringLiteral("Centre");
        two.rootPath = QStringLiteral("C:/tmp");
        two.capturedAt = QDateTime::currentDateTime();
        two.cellSize = QSize(75, 98);
        two.entries << entry(QStringLiteral("a.txt"), Kind::Document, QStringLiteral("txt"));
        two.entries << entry(QStringLiteral("b.zip"), Kind::Archive, QStringLiteral("zip"));
        two.entries << entry(QStringLiteral("c.exe"), Kind::Executable, QStringLiteral("exe"));
        shot.presentSession(two);

        const QList<FilterChip *> drawn =
            shot.findChildren<FilterChip *>(QStringLiteral("filterChip"));
        check(!drawn.isEmpty(), QStringLiteral("chips exist to measure"));
        for (FilterChip *chip : drawn) {
            const QImage img = chip->grab().toImage();

            const int inset = 5;
            QHash<QRgb, int> histogram;
            for (int y = inset; y < img.height() - inset; ++y)
                for (int x = inset; x < img.width() - inset; ++x)
                    ++histogram[img.pixel(x, y) & 0x00ffffff];

            QRgb fillRgb = 0;
            int best = -1;
            for (auto it = histogram.constBegin(); it != histogram.constEnd(); ++it) {
                if (it.value() > best) {
                    best = it.value();
                    fillRgb = it.key();
                }
            }
            const QColor fill(fillRgb);

            int minX = img.width();
            int maxX = -1;
            for (int y = inset; y < img.height() - inset; ++y) {
                for (int x = inset; x < img.width() - inset; ++x) {
                    const QColor c = img.pixelColor(x, y);

                    const int d = qAbs(c.red() - fill.red())
                                + qAbs(c.green() - fill.green())
                                + qAbs(c.blue() - fill.blue());
                    if (d > 8) {
                        minX = qMin(minX, x);
                        maxX = qMax(maxX, x);
                    }
                }
            }
            if (maxX < 0) {
                check(false, QStringLiteral("chip \"%1\" drew text").arg(chip->label()));
                continue;
            }

            const int left = minX - inset;
            const int right = (img.width() - 1 - inset) - maxX;
            check(left > 1 && right > 1,
                  QStringLiteral("chip \"%1\" text sits inside the pill (left %2, right %3)")
                      .arg(chip->label())
                      .arg(left)
                      .arg(right));

            check(qAbs(left - right) <= 1,
                  QStringLiteral("chip \"%1\" text is centred (left %2, right %3)")
                      .arg(chip->label())
                      .arg(left)
                      .arg(right));
        }
    }

    out << "\n" << (failures == 0 ? "FILTER PASSED\n" : "FILTER FAILED\n");
    out.flush();
    return failures == 0 ? 0 : 1;
}
