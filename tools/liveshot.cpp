
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QSettings>
#include <QStyleFactory>
#include <QTemporaryDir>
#include <QTimer>

#include "mainwindow.h"
#include "sessionmodel.h"
#include "sessionstore.h"
#include "theme.h"

static void pump(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

static void writeFile(const QString &path, const QByteArray &body)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(body);
}

static void save(QWidget *w, const QString &path)
{
    QImage img(w->size(), QImage::Format_ARGB32_Premultiplied);
    img.fill(QColor(QStringLiteral("#2b3138")));
    w->render(&img, QPoint(), QRegion(), QWidget::DrawChildren);
    img.save(path);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CairnLiveShot"));
    app.setOrganizationName(QStringLiteral("Cairn"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       QDir::tempPath() + QStringLiteral("/cairn-liveshot"));
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setStyleSheet(Theme::buildStyleSheet());

    QTemporaryDir tmp;
    const QString root = tmp.path();

    writeFile(root + QStringLiteral("/scanner.txt"), "notes");
    writeFile(root + QStringLiteral("/SnipEx_v1.4.34.zip"), "archive");
    writeFile(root + QStringLiteral("/Cairn.exe"), "binary");
    writeFile(root + QStringLiteral("/palette.svg"), "<svg/>");

    QDir().mkpath(QStringLiteral("preview"));

    MainWindow window;
    window.resize(1060, 680);
    window.presentSession(SessionStore::capture(root));
    window.show();
    pump(300);
    save(&window, QStringLiteral("preview/live-before.png"));

    writeFile(root + QStringLiteral("/just_appeared.png"), "image");
    writeFile(root + QStringLiteral("/new-folder-here.tmp"), "x");
    pump(1300);
    save(&window, QStringLiteral("preview/live-after.png"));

    window.hide();
    return 0;
}
