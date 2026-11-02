#include "mainwindow.h"
#include "sessionstore.h"
#include "sessionwindow.h"
#include "theme.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QStyleFactory>
#include <QTimer>

namespace {

// The app icon: the stacked-stone mark on a dark rounded tile, drawn rather than
// shipped as a binary asset so it always matches the accent colour.
QIcon buildAppIcon()
{
    QIcon icon;
    for (int size : {16, 24, 32, 48, 64, 128, 256}) {
        QPixmap pix(size, size);
        pix.fill(Qt::transparent);

        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal radius = size * 0.22;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#15181d")));
        p.drawRoundedRect(QRectF(0, 0, size, size), radius, radius);

        const int inset = qRound(size * 0.2);
        const QPixmap glyph = Icons::render(
            Icons::Id::Cairn, size - inset * 2, Theme::brand(), 1.0);
        p.drawPixmap(inset, inset, glyph);
        p.end();

        icon.addPixmap(pix);
    }
    return icon;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Cairn"));
    // No applicationDisplayName: Qt appends it to every window title, which
    // turned "Cairn — desktop-1k4f2a" into "… - Cairn" in the taskbar.
    app.setOrganizationName(QStringLiteral("Cairn"));
    app.setApplicationVersion(QStringLiteral("1.0"));
    app.setWindowIcon(buildAppIcon());
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setStyleSheet(Theme::buildStyleSheet());

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Dark, compact layout snapshots for folders."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption openOption(
        {QStringLiteral("o"), QStringLiteral("open")},
        QStringLiteral("Open a .session file or handle in its own window."),
        QStringLiteral("session"));
    parser.addOption(openOption);
    parser.process(app);

    const QStringList positional = parser.positionalArguments();

    // Opening a handle directly is how a session gets handed to someone else:
    // `cairn -o desktop-1k4f2a`, or by dragging the .session file onto the exe.
    QString toOpen = parser.value(openOption);
    if (toOpen.isEmpty() && !positional.isEmpty())
        toOpen = positional.first();

    if (!toOpen.isEmpty()) {
        auto *viewer = new SessionWindow;
        // A lone viewer is the whole app here, so closing it must end the
        // process. Without this the window disappeared and an invisible process
        // kept running.
        viewer->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(viewer, &QObject::destroyed, &app, &QApplication::quit);
        if (viewer->showSession(toOpen))
            return app.exec();
        delete viewer;
        return 1;
    }

    // First run: ask which folder to snapshot before the library appears.
    QSettings settings;
    const bool seenOnboarding =
        settings.value(QStringLiteral("onboarding/completed"), false).toBool();

    auto *window = new MainWindow;
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->show();

    // Closing the library ends the app, even if viewers are open: they were
    // opened from it and quitting with invisible windows left running is worse
    // than closing them too.
    QObject::connect(window, &QObject::destroyed, &app, &QApplication::quit);

    // A snapshot can always be opened in its own window, either from the
    // first-run dialog or from the library. The viewer loads from the saved
    // file, so it stays independent of the library's current selection.
    QObject::connect(window, &MainWindow::sessionOpened, window,
                     [](const QString &id) {
                         auto *viewer = new SessionWindow;
                         viewer->setAttribute(Qt::WA_DeleteOnClose);
                         if (!viewer->showSession(id))
                             viewer->deleteLater();
                     });

    if (!seenOnboarding) {
        QTimer::singleShot(0, window, [window] {
            const QString id = window->runSnapshotDialog(true);
            if (!id.isEmpty())
                QSettings().setValue(QStringLiteral("onboarding/completed"), true);
        });
    }

    return app.exec();
}
