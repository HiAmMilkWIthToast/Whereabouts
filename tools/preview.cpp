// Renders the app's windows to PNGs so the design can be inspected without a
// desktop session. Not part of the shipped app.
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QSettings>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QTimer>

#include "inspector.h"
#include "layoutview.h"
#include "onboarding.h"
#include "sessionstore.h"
#include "theme.h"
#include "mainwindow.h"
#include "sessionwindow.h"

namespace {

QString g_tracePath = QStringLiteral("preview/preview-trace.log");

void trace(const QString &text)
{
    QFile file(g_tracePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append))
        file.write((text + QLatin1Char('\n')).toUtf8());
}

// A folder that represents a real desktop: a mix of the kinds the app exists to
// distinguish, so the map is exercised properly.
Session sampleSession()
{
    Session session;
    session.id = QStringLiteral("desktop-1k4f2a");
    session.name = QStringLiteral("Desktop");
    session.rootPath = QStringLiteral("C:/Users/Kille/OneDrive/Desktop");
    session.capturedAt = QDateTime::currentDateTime();
    session.screen = QSize(2560, 1440);
    session.cellSize = QSize(75, 98);
    session.livePositions = true;

    struct Spec {
        const char *name;
        Kind kind;
        const char *suffix;
        qint64 size;
        int col, row;
        bool live;
    };

    const Spec specs[] = {
        {"SnipEx_v1.4.34.zip",   Kind::Archive,    "zip",  48230000, 1, 0, true},
        {"working pointers.png", Kind::Image,      "png",  1843200,  1, 1, true},
        {"scanner.txt",          Kind::Document,   "txt",  2140,     0, 0, true},
        {"idea1",                Kind::Folder,     "dir",  73400320, 0, 2, true},
        {"build.bat",            Kind::Executable, "bat",  420,      2, 1, true},
        {"Cairn.exe",            Kind::Executable, "exe",  9180000,  2, 0, true},
        {"layout-notes.md",      Kind::Document,   "md",   5120,     2, 2, true},
        {"palette.svg",          Kind::Image,      "svg",  3320,     1, 3, true},
        {"session.cpp",          Kind::Code,       "cpp",  22100,    0, 3, true},
        {"archive.7z",           Kind::Archive,    "7z",   1104000,  2, 3, true},
        {"track.flac",           Kind::Audio,      "flac",  41900000, 3, 1, true},
        {"demo.mp4",             Kind::Video,      "mp4",  96400000, 3, 2, true},
    };

    for (const Spec &spec : specs) {
        Entry entry;
        entry.name = QString::fromLatin1(spec.name);
        entry.relativePath = entry.name;
        entry.kind = spec.kind;
        entry.suffix = QString::fromLatin1(spec.suffix);
        entry.size = spec.size;
        entry.modified = QDateTime::currentDateTime().addDays(-spec.row * 3);
        entry.pos = QPoint(spec.col * 75 + 8, spec.row * 98 + 2);
        entry.cell = QPoint(spec.col, spec.row);
        entry.fromShell = spec.live;
        if (spec.kind == Kind::Folder)
            entry.children = 14;
        session.entries.append(entry);
    }

    // One icon dragged far away, which is what a real desktop usually looks
    // like. The map must collapse the empty columns rather than shrink every
    // tile to fit them.
    Entry stray;
    stray.name = QStringLiteral("idea1");
    stray.relativePath = stray.name;
    stray.kind = Kind::Folder;
    stray.suffix = QStringLiteral("dir");
    stray.children = 14;
    stray.pos = QPoint(913, 590);
    stray.cell = QPoint(12, 6);
    stray.fromShell = true;
    session.entries.replace(3, stray);

    return session;
}

void save(QWidget *widget, const QString &path, const QColor &backdrop)
{
    const QSize size = widget->size();
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(backdrop);

    widget->render(&image, QPoint(), QRegion(), QWidget::DrawChildren);
    image.save(path);
}

void pump(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CairnPreview"));
    app.setOrganizationName(QStringLiteral("Cairn"));

    // Keep the preview from inheriting or overwriting the real app's settings.
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       QDir::tempPath() + QStringLiteral("/cairn-preview"));
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setStyleSheet(Theme::buildStyleSheet());

    const QColor backdrop(QStringLiteral("#2b3138"));
    // Its own folder, so repeated runs never litter the build directory.
    const QString out = QStringLiteral("preview");
    QDir().mkpath(out);

    trace(QStringLiteral("start"));

    // ---- detail view, map mode, with an entry selected
    {
        trace(QStringLiteral("main:construct"));
        MainWindow window;
        window.resize(1060, 680);

        auto *layout = window.findChild<LayoutView *>();
        auto *inspector = window.findChild<Inspector *>();
        trace(QStringLiteral("main:setSession layout=%1 inspector=%2")
                  .arg(layout != nullptr).arg(inspector != nullptr));
        const Session session = sampleSession();
        window.presentSession(session);
        trace(QStringLiteral("main:presented"));

        // Select an entry so the inspector shows an item rather than the summary.
        if (inspector) {
            inspector->setEntry(&session.entries.at(0));
            trace(QStringLiteral("main:inspector set"));
        }
        window.show();
        trace(QStringLiteral("main:shown"));
        pump(150);
        trace(QStringLiteral("main:pump done"));
        save(&window, out + QStringLiteral("/preview-main.png"), backdrop);
        trace(QStringLiteral("main:saved"));
        window.hide();
    }

    // ---- the layout map on its own, large, so the tiles can be judged
    {
        auto *view = new LayoutView;
        view->resize(760, 470);
        view->setSession(sampleSession());
        view->show();
        pump(80);
        save(view, out + QStringLiteral("/preview-map.png"), backdrop);
        delete view;
        trace(QStringLiteral("map:saved"));
    }

    // ---- list mode
    {
        auto *view = new LayoutView;
        view->resize(560, 420);
        view->setSession(sampleSession());
        view->setMode(LayoutView::Mode::List);
        view->show();
        pump(80);
        save(view, out + QStringLiteral("/preview-list.png"), backdrop);
        delete view;
        trace(QStringLiteral("list:saved"));
    }

    // ---- inspector on its own
    {
        auto *inspector = new Inspector;
        inspector->resize(Metrics::kInspectorWidth, 470);
        const Session session = sampleSession();
        inspector->setSession(session);
        inspector->setEntry(&session.entries.at(0));
        inspector->show();
        pump(60);
        save(inspector, out + QStringLiteral("/preview-inspector.png"), backdrop);
        delete inspector;
        trace(QStringLiteral("inspector:saved"));
    }

    // ---- first-run dialog
    {
        OnboardingDialog dialog(SessionStore::desktopPath(), true);
        dialog.show();
        pump(60);
        save(&dialog, out + QStringLiteral("/preview-onboarding.png"), backdrop);
        dialog.hide();
        trace(QStringLiteral("onboarding:saved"));
    }

    trace(QStringLiteral("done"));
    return 0;
}
