// Clicks the real title-bar buttons on each top-level window and reports whether
// the window actually minimized / closed. Development helper only.
//
// Windows are moved off-screen so this does not flash in front of the user.
#include <QApplication>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QStyleFactory>
#include <QTextStream>
#include <QTimer>
#include <QWidget>

#include "chrome.h"
#include "mainwindow.h"
#include "sessionstore.h"
#include "sessionwindow.h"
#include "theme.h"

static QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

static int failures = 0;

static void check(bool condition, const QString &what)
{
    out() << (condition ? "  PASS  " : "  FAIL  ") << what << "\n";
    out().flush();
    if (!condition)
        ++failures;
}

// Presses and releases a widget with left-button events, exactly what a user's
// click produces once it reaches the widget.
static void click(QWidget *widget)
{
    const QPointF centre(widget->width() / 2.0, widget->height() / 2.0);
    QMouseEvent press(QEvent::MouseButtonPress, centre, widget->mapToGlobal(centre),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(widget, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, centre, widget->mapToGlobal(centre),
                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(widget, &release);
}

// The three window-control buttons, in the order the title bar creates them.
static QList<IconButton *> controls(QWidget *window)
{
    auto *bar = window->findChild<TitleBar *>();
    if (!bar)
        return {};
    return bar->findChildren<IconButton *>(QString(), Qt::FindDirectChildrenOnly);
}

static void pump(int ms)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
}

static void probe(const QString &label, QWidget *window)
{
    out() << "\n== " << label << " ==\n";
    window->move(-4000, -4000);
    window->show();
    pump(300);

    const QList<IconButton *> buttons = controls(window);
    check(buttons.size() == 3,
          QStringLiteral("title bar exposes 3 controls (found %1)").arg(buttons.size()));
    if (buttons.size() != 3) {
        window->close();
        return;
    }

    // Minimize
    click(buttons.at(0));
    pump(250);
    check(window->isMinimized(), QStringLiteral("minimize button minimizes the window"));

    window->showNormal();
    pump(250);

    // Maximize. The glyph must follow the state, or the button lies.
    click(buttons.at(1));
    pump(250);
    check(window->isMaximized(), QStringLiteral("maximize button maximizes the window"));
    check(buttons.at(1)->icon() == Icons::Id::Restore,
          QStringLiteral("maximize glyph becomes Restore when maximized"));

    window->showNormal();
    pump(250);
    check(buttons.at(1)->icon() == Icons::Id::Maximize,
          QStringLiteral("maximize glyph returns when restored"));

    // Close
    click(buttons.at(2));
    pump(250);
    check(!window->isVisible(), QStringLiteral("close button closes the window"));
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CairnWindowProbe"));
    app.setOrganizationName(QStringLiteral("CairnWindowProbe"));
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setStyleSheet(Theme::buildStyleSheet());

    {
        auto *main = new MainWindow;
        probe(QStringLiteral("MainWindow"), main);
        delete main;
    }

    {
        auto *viewer = new SessionWindow;
        probe(QStringLiteral("SessionWindow"), viewer);
        delete viewer;
    }

    // Advertised shortcuts must actually do something. Ctrl+F on the viewer is
    // the cheapest one to observe: it must move focus into the search field.
    {
        auto *viewer = new SessionWindow;
        viewer->move(-4000, -4000);
        viewer->show();
        pump(300);

        out() << "\n== shortcuts ==\n";
        QKeyEvent press(QEvent::KeyPress, Qt::Key_F, Qt::ControlModifier);
        QApplication::sendEvent(viewer, &press);
        pump(150);
        // Asked of the viewer, not of the application. The global focusWidget()
        // is only non-null while that window holds activation, and this window
        // is placed off-screen where the window manager may never activate it --
        // the event still reaches the handler, so the global answer was a coin
        // toss that failed perhaps one run in twenty.
        auto *focused = qobject_cast<QLineEdit *>(viewer->focusWidget());
        check(focused != nullptr,
              QStringLiteral("Ctrl+F focuses a text field in the viewer"));
        viewer->close();
        pump(150);
        delete viewer;
    }

    out() << (failures == 0 ? "\nALL CHECKS PASSED\n"
                            : QStringLiteral("\n%1 CHECK(S) FAILED\n").arg(failures));
    return failures == 0 ? 0 : 1;
}
