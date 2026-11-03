
#include <QApplication>
#include <QComboBox>
#include <QEventLoop>
#include <QImage>
#include <QKeyEvent>
#include <QPushButton>
#include <QStyleFactory>
#include <QTimer>

#include "mainwindow.h"
#include "sessionmodel.h"
#include "theme.h"

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
    app.setApplicationName(QStringLiteral("CairnShot"));
    app.setOrganizationName(QStringLiteral("CairnShot"));

    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setStyleSheet(Theme::buildStyleSheet());

    Session s;
    s.id = QStringLiteral("shot");
    s.name = QStringLiteral("Desktop");
    s.rootPath = QStringLiteral("C:/tmp");
    s.capturedAt = QDateTime::currentDateTime();
    s.cellSize = QSize(75, 98);
    s.entries << entry(QStringLiteral("a.txt"), Kind::Document, QStringLiteral("txt"));
    s.entries << entry(QStringLiteral("b.zip"), Kind::Archive, QStringLiteral("zip"));
    s.entries << entry(QStringLiteral("c.exe"), Kind::Executable, QStringLiteral("exe"));

    MainWindow window;
    window.resize(900, 620);
    window.presentSession(s);
    window.show();
    pump(300);

    {
        QImage detail(window.width(), window.height(),
                     QImage::Format_ARGB32_Premultiplied);
        detail.fill(QColor(QStringLiteral("#0b0d10")));
        window.render(&detail, QPoint(), QRegion(), QWidget::DrawChildren);
        detail.save(QStringLiteral("C:/Users/Kille/MonkeyCode/shot_detail.png"));
    }

    {
        QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(&window, &esc);
        pump(200);
    }

    {
        QImage lib(window.width(), window.height(),
                   QImage::Format_ARGB32_Premultiplied);
        lib.fill(QColor(QStringLiteral("#0b0d10")));
        window.render(&lib, QPoint(), QRegion(), QWidget::DrawChildren);
        lib.save(QStringLiteral("C:/Users/Kille/MonkeyCode/shot_library.png"));
    }

    window.presentSession(s);
    pump(200);

    for (QPushButton *b : window.findChildren<QPushButton *>()) {
        if (b->text() == QStringLiteral("List")) {
            b->click();
            break;
        }
    }
    pump(250);

    QImage img(window.width(), window.height(), QImage::Format_ARGB32_Premultiplied);
    img.fill(QColor(QStringLiteral("#0b0d10")));
    window.render(&img, QPoint(), QRegion(), QWidget::DrawChildren);
    img.save(QStringLiteral("C:/Users/Kille/MonkeyCode/shot_header.png"));

    if (auto *box = window.findChild<QComboBox *>(QStringLiteral("field"))) {
        box->grab().save(QStringLiteral("C:/Users/Kille/MonkeyCode/shot_combo.png"));
    }
    return 0;
}
