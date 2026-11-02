// Reproduces the sort popup so its styling can be inspected. Dev helper only.
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QEventLoop>
#include <QImage>
#include <QPushButton>
#include <QStyleFactory>
#include <QTextStream>
#include <QTimer>

#include "mainwindow.h"
#include "sessionmodel.h"
#include "theme.h"

static QTextStream out(stdout);

static void pump(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
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
    app.setApplicationName(QStringLiteral("CairnPopup"));
    app.setOrganizationName(QStringLiteral("CairnPopup"));
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setStyleSheet(Theme::buildStyleSheet());

    Session s;
    s.id = QStringLiteral("popup");
    s.name = QStringLiteral("Desktop");
    s.rootPath = QStringLiteral("C:/tmp");
    s.capturedAt = QDateTime::currentDateTime();
    s.cellSize = QSize(75, 98);
    s.entries << entry(QStringLiteral("a.txt"), Kind::Document, QStringLiteral("txt"));
    s.entries << entry(QStringLiteral("b.zip"), Kind::Archive, QStringLiteral("zip"));

    MainWindow window;
    window.resize(900, 620);
    window.presentSession(s);
    window.show();
    pump(300);

    for (QPushButton *b : window.findChildren<QPushButton *>()) {
        if (b->text() == QStringLiteral("List")) {
            b->click();
            break;
        }
    }
    pump(200);

    auto *box = window.findChild<QComboBox *>(QStringLiteral("field"));
    if (!box) {
        out << "no combo\n";
        return 1;
    }

    QAbstractItemView *view = box->view();
    out << "view class      : "
        << QString::fromLatin1(view->metaObject()->className()) << "\n";

    QWidget *container = view->parentWidget();
    if (container) {
        out << "container class : "
            << QString::fromLatin1(container->metaObject()->className()) << "\n";
    }

    box->showPopup();
    pump(400);

    // The frame around the list is the private container, not the view. Half of
    // the light area in the report was this, so both are captured.
    view->grab().save(QStringLiteral("C:/Users/Kille/MonkeyCode/shot_popup_view.png"));
    if (container) {
        container->grab().save(
            QStringLiteral("C:/Users/Kille/MonkeyCode/shot_popup_frame.png"));
        out << "container size  : " << container->width() << "x"
            << container->height() << "\n";
    }
    out << "view size       : " << view->width() << "x" << view->height() << "\n";
    out.flush();
    return 0;
}
