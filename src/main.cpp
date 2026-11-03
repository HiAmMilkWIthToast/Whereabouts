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

}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Cairn"));

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

    QString toOpen = parser.value(openOption);
    if (toOpen.isEmpty() && !positional.isEmpty())
        toOpen = positional.first();

    if (!toOpen.isEmpty()) {
        auto *viewer = new SessionWindow;

        viewer->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(viewer, &QObject::destroyed, &app, &QApplication::quit);
        if (viewer->showSession(toOpen))
            return app.exec();
        delete viewer;
        return 1;
    }

    QSettings settings;
    const bool seenOnboarding =
        settings.value(QStringLiteral("onboarding/completed"), false).toBool();

    auto *window = new MainWindow;
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->show();

    QObject::connect(window, &QObject::destroyed, &app, &QApplication::quit);

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
