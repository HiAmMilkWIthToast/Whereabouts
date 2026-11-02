// Checks that every icon in the enum actually has artwork, and that none of it
// is an empty or unparsable path. Development helper only.
//
// svgFor() is a switch with no default, so adding an Id and forgetting the case
// compiles cleanly and renders nothing: an invisible button, with no warning.
// That is the failure this catches.
#include <QApplication>
#include <QImage>
#include <QPixmap>
#include <QTextStream>

#include <QEventLoop>
#include <QTimer>
#include <QEnterEvent>
#include <QFontMetrics>
#include <QDateTime>
#include "sessionmodel.h"
#include "mainwindow.h"
#include "icontextbutton.h"
#include <QVector>

#include "icons.h"
#include "theme.h"

static QTextStream out(stdout);
static int failures = 0;

static void check(bool ok, const QString &what)
{
    out << (ok ? "  PASS  " : "  FAIL  ") << what << QLatin1Char(10);
    if (!ok)
        ++failures;
}


// A small session built in memory: enough kinds to populate the header, and
// nothing read from disk, so the check works on a clean machine.
static Session iconcheckSession()
{
    static const auto make = [] {
        Session s;
        s.name = QStringLiteral("iconcheck");
        s.rootPath = QStringLiteral("C:/iconcheck");
        s.capturedAt = QDateTime::currentDateTime();
        s.screen = QSize(1920, 1080);
        s.cellSize = QSize(96, 96);
        const Kind kinds[] = {Kind::Folder, Kind::Archive, Kind::Document};
        for (int i = 0; i < 3; ++i) {
            Entry e;
            e.name = QStringLiteral("item%1").arg(i);
            e.relativePath = e.name;
            e.kind = kinds[i];
            e.size = 1024 * (i + 1);
            e.modified = s.capturedAt;
            e.cell = QPoint(i, 0);
            s.entries << e;
        }
        return s;
    }();
    return make;
}


static QString nameOf(Icons::Id id)
{
    switch (id) {
    case Icons::Id::Cairn: return QStringLiteral("Cairn");
    case Icons::Id::CairnCompact: return QStringLiteral("CairnCompact");
    case Icons::Id::Layout: return QStringLiteral("Layout");
    case Icons::Id::Folder: return QStringLiteral("Folder");
    case Icons::Id::File: return QStringLiteral("File");
    case Icons::Id::Archive: return QStringLiteral("Archive");
    case Icons::Id::Executable: return QStringLiteral("Executable");
    case Icons::Id::Image: return QStringLiteral("Image");
    case Icons::Id::Code: return QStringLiteral("Code");
    case Icons::Id::Text: return QStringLiteral("Text");
    case Icons::Id::Link: return QStringLiteral("Link");
    case Icons::Id::Search: return QStringLiteral("Search");
    case Icons::Id::Plus: return QStringLiteral("Plus");
    case Icons::Id::Info: return QStringLiteral("Info");
    case Icons::Id::Grid: return QStringLiteral("Grid");
    case Icons::Id::List: return QStringLiteral("List");
    case Icons::Id::Home: return QStringLiteral("Home");
    case Icons::Id::Export: return QStringLiteral("Export");
    case Icons::Id::SortAsc: return QStringLiteral("SortAsc");
    case Icons::Id::SortDesc: return QStringLiteral("SortDesc");
    case Icons::Id::Close: return QStringLiteral("Close");
    case Icons::Id::Minimize: return QStringLiteral("Minimize");
    case Icons::Id::Maximize: return QStringLiteral("Maximize");
    case Icons::Id::Restore: return QStringLiteral("Restore");
    }
    return QStringLiteral("?");
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // The list is written out rather than derived, because the enum has no
    // sentinel to count to. If an Id is added and not listed here, the count
    // check below runs one short -- so the enum size is asserted too.
    const QList<Icons::Id> all = {
        Icons::Id::Cairn, Icons::Id::CairnCompact, Icons::Id::Layout,
        Icons::Id::Folder, Icons::Id::File, Icons::Id::Archive,
        Icons::Id::Executable, Icons::Id::Image, Icons::Id::Code,
        Icons::Id::Text, Icons::Id::Link, Icons::Id::Search,
        Icons::Id::Plus, Icons::Id::Info, Icons::Id::Grid, Icons::Id::List,
        Icons::Id::Home, Icons::Id::Export, Icons::Id::SortAsc,
        Icons::Id::SortDesc, Icons::Id::Close, Icons::Id::Minimize,
        Icons::Id::Maximize, Icons::Id::Restore,
    };

    out << "== every icon has artwork ==" << QLatin1Char(10);
    for (Icons::Id id : all) {
        const QString svg = QString::fromUtf8(Icons::svgFor(id));
        check(!svg.isEmpty(), QStringLiteral("%1 has an SVG").arg(nameOf(id)));
        if (svg.isEmpty())
            continue;

        // Parsed, not just non-empty: a malformed path renders blank in the UI
        // while the string looks fine.
        const QPixmap mask = Icons::alphaMask(id, 24);
        QImage img = mask.toImage();
        bool inked = false;
        for (int y = 0; y < img.height() && !inked; ++y) {
            for (int x = 0; x < img.width() && !inked; ++x) {
                if (img.pixelColor(x, y).alpha() > 8)
                    inked = true;
            }
        }
        check(inked, QStringLiteral("%1 actually draws something").arg(nameOf(id)));
    }

    out << QLatin1Char(10) << "== the list matches the enum ==" << QLatin1Char(10);
    check(all.size() == int(Icons::Id::Restore) + 1,
          QStringLiteral("the check lists all %1 ids").arg(int(Icons::Id::Restore) + 1));

    out << QLatin1Char(10) << "== glyphs are legible when small ==" << QLatin1Char(10);
    {
        // The UI uses these at 12 to 17px. Below that they are never drawn, so
        // that is the floor worth asserting.
        bool ok = true;
        for (Icons::Id id : all) {
            const QPixmap p = Icons::render(id, 12, Theme::accent(), 1.0);
            if (p.isNull() || p.width() != 12) {
                out << "        (bad render: " << nameOf(id) << ")" << QLatin1Char(10);
                ok = false;
            }
        }
        check(ok, QStringLiteral("every icon renders at 12px"));
    }


    out << QLatin1Char(10) << "== the buttons that carry them ==" << QLatin1Char(10);
    {
        // Built through the real window so the object names, and therefore the
        // stylesheet rules behind the colours, are the ones the app uses.
        // The detail page, where every one of these buttons is on screen at
        // once; the library page hides most of them, so hovering there would
        // compare two blank images and pass while proving nothing.
        MainWindow window;
        window.resize(1000, 640);
        window.presentSession(iconcheckSession());
        window.show();
        QEventLoop loop;
        QTimer::singleShot(250, &loop, &QEventLoop::quit);
        loop.exec();

        const auto buttons = window.findChildren<IconTextButton *>();
        check(buttons.size() >= 5,
              QStringLiteral("the header buttons carry icons (%1)")
                  .arg(buttons.size()));

        // Only a shown widget paints: grabbing a hidden one returns the same
        // blank image twice and the comparison proves nothing.
        int hovered = 0;
        for (IconTextButton *b : buttons) {
            check(!b->toolTip().isEmpty(),
                  QStringLiteral("#%1 explains itself: \"%2\"")
                      .arg(b->objectName(), b->toolTip()));
            if (!b->isVisible())
                continue;

            // The colour the glyph and label are painted with must change on
            // hover. Sampling rendered pixels cannot show this reliably: the
            // sample always includes the button's background, which the
            // stylesheet repaints whether or not the icon follows it.
            const QColor rest = b->inkColor();
            QEnterEvent enter(QPointF(2, 2), b->mapToGlobal(QPointF(2, 2)),
                              b->mapToGlobal(QPointF(2, 2)));
            QApplication::sendEvent(b, &enter);
            const QColor after = b->inkColor();
            check(rest != after,
                  QStringLiteral("#%1's icon follows the pointer (%2 -> %3)")
                      .arg(b->objectName(), rest.name(), after.name()));
            ++hovered;
        }
        // The library page hides the detail-page controls; if that ever became
        // every button, the loop above would pass while checking nothing.
        check(hovered >= 3,
              QStringLiteral("that was checked on %1 visible buttons").arg(hovered));
    }

    out << QLatin1Char(10) << "== the glyph does not collide with the word ==" << QLatin1Char(10);
    {
        // A glyph drawn over its label is invisible in a screenshot until the
        // strings change. Assert the geometry instead of trusting the layout.
        IconTextButton probe(Icons::Id::Export, QStringLiteral("Export"));
        probe.setObjectName(QStringLiteral("secondary"));
        const QFontMetrics fm(probe.font());
        check(probe.sizeHint().width() > probe.glyphWidth() + fm.horizontalAdvance(QStringLiteral("Export")),
              QStringLiteral("there is room for both, with padding"));
        check(probe.glyphWidth() >= 12,
              QStringLiteral("the glyph is at least 12px (it is %1)")
                  .arg(probe.glyphWidth()));
    }
    out << QLatin1Char(10);
    out << (failures == 0 ? "ICONCHECK PASSED" : "ICONCHECK FAILED") << QLatin1Char(10);
    out.flush();
    return failures == 0 ? 0 : 1;
}
