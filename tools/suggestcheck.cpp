// Checks the search box's snapshot list. Development helper only.
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QTextStream>

#include "elidedlabel.h"
#include "mainwindow.h"
#include "theme.h"
#include <QPalette>
#include <QStyle>
#include <QPushButton>
#include <QRegularExpression>
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

static Entry ent(const QString &name, Kind kind, const QString &suffix)
{
    Entry e;
    e.name = name;
    e.relativePath = name;
    e.kind = kind;
    e.suffix = suffix;
    e.cell = QPoint(0, 0);
    return e;
}

static Session makeSession(const QString &id, const QString &name)
{
    Session s;
    s.id = id;
    s.name = name;
    s.rootPath = QStringLiteral("C:/tmp");
    s.capturedAt = QDateTime::currentDateTime();
    s.cellSize = QSize(75, 98);
    s.entries << ent(QStringLiteral("a.txt"), Kind::Document, QStringLiteral("txt"));
    return s;
}

// The popup is driven through the real text box, so the checks exercise the
// path a person takes rather than calling the helper directly.
static void type(QLineEdit *box, const QString &text)
{
    box->setText(text);
    QApplication::processEvents();
}

static QStringList names(QListView *list)
{
    QStringList out;
    auto *model = qobject_cast<QStandardItemModel *>(list->model());
    if (!model)
        return out;
    for (int i = 0; i < model->rowCount(); ++i)
        out << model->item(i)->text();
    return out;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CairnSuggestTest"));
    app.setOrganizationName(QStringLiteral("CairnSuggestTest"));

    // Written into this app's own sessions folder, so the run never touches the
    // user's library.
    const QStringList ids = {QStringLiteral("suggest-alpha"),
                             QStringLiteral("suggest-beta"),
                             QStringLiteral("suggest-gamma")};
    const QStringList labels = {QStringLiteral("Alpha desktop"),
                                QStringLiteral("Beta desktop"),
                                QStringLiteral("Gamma desktop")};
    QStringList written;
    for (int i = 0; i < ids.size(); ++i) {
        const QString path = SessionStore::pathForId(ids.at(i));
        if (SessionStore::save(makeSession(ids.at(i), labels.at(i))))
            written << path;
    }

    MainWindow window;
    window.resize(900, 620);
    window.show();
    QApplication::processEvents();

    QLineEdit *box = nullptr;
    for (QLineEdit *candidate : window.findChildren<QLineEdit *>()) {
        if (candidate->property("role").toString()
            == QStringLiteral("sessionSearch")) {
            box = candidate;
            break;
        }
    }
    auto *list = window.findChild<QListView *>(QStringLiteral("suggestionList"));
    check(box != nullptr, QStringLiteral("the search box exists"));
    if (box) {
        out << "  info  placeholder: [" << box->placeholderText() << "]" << QLatin1Char(10);
        out << "  info  parent     : ["
            << (box->parentWidget() ? box->parentWidget()->objectName() : QString())
            << "]" << QLatin1Char(10);
    }
    check(list != nullptr, QStringLiteral("it has a suggestion list"));
    if (!box || !list) {
        out << "\nSUGGEST FAILED\n";
        return 1;
    }

    out << "\n== the list offers the whole library ==\n";
    {
        type(box, QString());
        const QStringList all = names(list);
        check(all.size() >= 3,
              QStringLiteral("every stored snapshot is offered (%1)").arg(all.size()));
        check(all.contains(labels.at(0)) && all.contains(labels.at(1))
                  && all.contains(labels.at(2)),
              QStringLiteral("the names shown are the session names"));
    }

    out << "\n== typing narrows it ==\n";
    {
        type(box, QStringLiteral("beta"));
        const QStringList narrowed = names(list);
        check(narrowed.size() == 1,
              QStringLiteral("one match for \"beta\" (%1)").arg(narrowed.size()));
        check(narrowed.value(0) == labels.at(1),
              QStringLiteral("and it is the right one (%1)").arg(narrowed.value(0)));
    }

    out << "\n== the list matches what the main list shows ==\n";
    {
        type(box, QStringLiteral("desktop"));
        QStringList visible;
        auto *widgets = window.findChild<QListWidget *>(QStringLiteral("sessionList"));
        check(widgets != nullptr, QStringLiteral("the session list exists"));
        if (widgets) {
            for (int i = 0; i < widgets->count(); ++i) {
                QListWidgetItem *item = widgets->item(i);
                if (!item->isHidden())
                    visible << item->data(Qt::UserRole + 1).toString();
            }
        }
        const QStringList offered = names(list);
        // Both are sorted from the same source, so they must agree exactly; a
        // mismatch would mean the popup is one refresh behind.
        check(offered == visible,
              QStringLiteral("same entries as the list (%1 vs %2)")
                  .arg(offered.size()).arg(visible.size()));
    }

    out << "\n== nothing matching explains itself ==\n";
    {
        type(box, QStringLiteral("zzz-no-such-snapshot"));
        const QStringList none = names(list);
        check(none.size() == 1, QStringLiteral("one placeholder row (%1)").arg(none.size()));
        check(none.value(0).contains(QStringLiteral("match"), Qt::CaseInsensitive),
              QStringLiteral("the placeholder says why (%1)").arg(none.value(0)));

        // The placeholder must not be pickable: it carries no id.
        auto *model = qobject_cast<QStandardItemModel *>(list->model());
        check(model && model->item(0)->data(Qt::UserRole).toString().isEmpty(),
              QStringLiteral("the placeholder opens nothing"));
        check(model && !model->item(0)->isEnabled(),
              QStringLiteral("the placeholder is not selectable"));
    }

    out << "== the subtitle ellipsizes instead of being cut ==" << QLatin1Char(10);
    {
        // The stored path is long and the page is not always wide. At the
        // minimum window size the text used to run on under the search box and
        // be clipped mid-word.
        auto *subtitle = window.findChild<QLabel *>(QStringLiteral("librarySubtitle"));
        check(subtitle != nullptr, QStringLiteral("the subtitle exists"));
        if (subtitle) {
            auto *elided = qobject_cast<ElidedLabel *>(subtitle);
            check(elided != nullptr, QStringLiteral("it is an eliding label"));
            const QString full = elided ? elided->fullText() : QString();
            check(!full.isEmpty(), QStringLiteral("the full text is kept (%1 chars)")
                                      .arg(full.size()));
            check(subtitle->toolTip() == full,
                  QStringLiteral("the whole path is available on hover"));

            // Narrowing the window must shorten the text, not widen the header.
            window.resize(560, 620);
            QApplication::processEvents();
            const int narrowWidth = subtitle->width();
            check(narrowWidth < subtitle->fontMetrics().horizontalAdvance(full),
                  QStringLiteral("the narrow label is narrower than its text (%1 of %2)")
                      .arg(narrowWidth)
                      .arg(subtitle->fontMetrics().horizontalAdvance(full)));

            // Widening gives back some of the text, though not necessarily all
            // of it: this test's own path is longer than the shipped app's, and
            // whether 900px is enough is a fact about the path, not a fault.
            window.resize(900, 620);
            QApplication::processEvents();
            check(subtitle->width() > narrowWidth,
                  QStringLiteral("widening gives it more room (%1 then %2)")
                      .arg(narrowWidth).arg(subtitle->width()));

            // Push it wide enough that no path could be cut: the label must
            // then take the room the layout offers rather than staying put.
            window.resize(1600, 620);
            QApplication::processEvents();
            check(subtitle->width() >= subtitle->fontMetrics().horizontalAdvance(full),
                  QStringLiteral("a wide window fits the whole path (%1 of %2)")
                      .arg(subtitle->width())
                      .arg(subtitle->fontMetrics().horizontalAdvance(full)));
        }
    }


    out << "\n== picking one opens it ==\n";
    {
        type(box, QStringLiteral("gamma"));
        auto *model = qobject_cast<QStandardItemModel *>(list->model());
        check(model && model->rowCount() == 1,
              QStringLiteral("gamma is the only match (%1)")
                  .arg(model ? model->rowCount() : -1));

        // Clicking the row is what a person does; drive the same signal.
        if (model && model->rowCount() == 1)
            emit list->clicked(model->index(0, 0));
        QApplication::processEvents();

        check(box->text().isEmpty(),
              QStringLiteral("the search box is cleared after picking"));
        check(!list->isVisible(), QStringLiteral("the list closes after picking"));

        // Opening moves the window to the detail page, which is where an opened
        // snapshot lives; the library page is index 0.
        auto *stack = window.findChild<QStackedWidget *>(QStringLiteral("pages"));
        check(stack != nullptr, QStringLiteral("the page stack exists"));
        if (stack) {
            check(stack->currentIndex() == 1,
                  QStringLiteral("picking a snapshot opens it (page %1)")
                      .arg(stack->currentIndex()));
        }
    }

    out << QLatin1Char(10) << "== the way back is gold, not grey ==" << QLatin1Char(10);
    {
        // The button is the one control that leaves the snapshot, so it is
        // styled apart from the neutral ghost buttons beside it.
        QPushButton *back = nullptr;
        for (QPushButton *b : window.findChildren<QPushButton *>()) {
            if (b->text() == QStringLiteral("Sessions")) {
                back = b;
                break;
            }
        }
        check(back != nullptr, QStringLiteral("the Sessions button exists"));
        if (back) {
            check(back->objectName() == QStringLiteral("backLink"),
                  QStringLiteral("it uses the gold style, not #ghost (%1)")
                      .arg(back->objectName()));

            const QString sheet = Theme::buildStyleSheet();
            check(sheet.contains(QStringLiteral("QPushButton#backLink")),
                  QStringLiteral("the stylesheet styles it"));

            // The colour is parsed by the style sheet at paint time, so the
            // widget's palette does not carry it; read the rule itself.
            int at = sheet.indexOf(QStringLiteral("QPushButton#backLink {"));
            check(at >= 0, QStringLiteral("the rule is findable"));
            if (at >= 0) {
                const int close = sheet.indexOf(QLatin1Char('}'), at);
                const QString rule = sheet.mid(at, close - at);

                const QRegularExpression colorRe(
                    QStringLiteral("color:[ ]*(#[0-9a-fA-F]{6})"));
                const auto m = colorRe.match(rule);
                check(m.hasMatch(), QStringLiteral("the rule sets a colour"));
                if (m.hasMatch()) {
                    const QColor text(m.captured(1));
                    // Gold: red leads, green follows, blue trails.
                    check(text.isValid() && text.red() > text.green()
                              && text.green() > text.blue(),
                          QStringLiteral("the text is a warm tone (%1)")
                              .arg(text.name()));
                    check(text.red() - text.blue() >= 40,
                          QStringLiteral("and reads as gold, not beige (%1)")
                              .arg(text.name()));
                }
            }
        }
    }

    out << QLatin1Char(10) << "== the logo is gold, and its file exists ==" << QLatin1Char(10);
    {
        // The mark was drawn with the interactive accent, which made the logo
        // read as another control. It has its own tone now.
        const QColor mark = Theme::brand();
        check(mark.red() > mark.green() && mark.green() > mark.blue(),
              QStringLiteral("the brand tone is warm (%1)").arg(mark.name()));

        // The same colour the Sessions button uses, so the two agree.
        const QString sheet = Theme::buildStyleSheet();
        check(sheet.contains(mark.name(), Qt::CaseInsensitive),
              QStringLiteral("the stylesheet uses the same tone (%1)")
                  .arg(mark.name()));

        // The committed icon files, which are what the resource compiles and
        // what a person sees in Explorer.
        const QStringList files = {
            QStringLiteral(CAIRN_RES_DIR "/cairn.ico"),
            QStringLiteral(CAIRN_RES_DIR "/cairn-256.png"),
        };
        for (const QString &f : files) {
            check(QFile::exists(f), QStringLiteral("present: %1").arg(f));
        }

        const QImage png(QStringLiteral(CAIRN_RES_DIR "/cairn-256.png"));
        check(!png.isNull() && png.width() == 256 && png.height() == 256,
              QStringLiteral("the PNG is 256x256 (%1x%2)")
                  .arg(png.width()).arg(png.height()));

        if (!png.isNull()) {
            // Somewhere in the middle of the tile there must be gold; a blank or
            // monochrome icon would pass a mere "file exists" check.
            bool foundGold = false;
            for (int y = 0; y < png.height() && !foundGold; y += 2) {
                for (int x = 0; x < png.width() && !foundGold; x += 2) {
                    const QColor c = png.pixelColor(x, y);
                    if (c.red() > 150 && c.green() > 110 && c.blue() < 130
                        && c.red() > c.blue() + 50) {
                        foundGold = true;
                    }
                }
            }
            check(foundGold, QStringLiteral("the icon is drawn in gold"));
            check(png.pixelColor(2, 2).alpha() == 0,
                  QStringLiteral("the corners are transparent, not a black box"));
        }
    }

    // Leave the machine as it was found.
    for (const QString &path : written)
        QFile::remove(path);

    out << "\n" << (failures == 0 ? "SUGGEST PASSED\n" : "SUGGEST FAILED\n");
    out.flush();
    return failures == 0 ? 0 : 1;
}
