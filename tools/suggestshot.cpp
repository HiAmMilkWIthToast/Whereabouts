
#include <QDir>
#include <QApplication>
#include <QImage>
#include <QLineEdit>
#include <QStyleFactory>
#include <QTimer>
#include <QEventLoop>
#include <QFile>
#include <QTextStream>

#include "mainwindow.h"
#include "sessionmodel.h"
#include "sessionstore.h"
#include "theme.h"

static QTextStream out(stdout);

static void pump(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

static Session makeSession(const QString &id, const QString &name, const QString &root)
{
    Session s;
    s.id = id;
    s.name = name;
    s.rootPath = root;
    s.capturedAt = QDateTime::currentDateTime();
    s.cellSize = QSize(75, 98);
    Entry e;
    e.name = QStringLiteral("a.txt");
    e.relativePath = e.name;
    e.kind = Kind::Document;
    e.suffix = QStringLiteral("txt");
    e.cell = QPoint(0, 0);
    s.entries << e;
    return s;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Cairn"));
    app.setOrganizationName(QStringLiteral("Cairn"));
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setStyleSheet(Theme::buildStyleSheet());

    const QStringList ids = {QStringLiteral("shot-one"), QStringLiteral("shot-two"),
                             QStringLiteral("shot-three")};
    const QStringList names = {QStringLiteral("Desktop"),
                               QStringLiteral("Downloads"),
                               QStringLiteral("Projects")};
    QStringList written;
    for (int i = 0; i < ids.size(); ++i) {
        if (SessionStore::save(makeSession(ids.at(i), names.at(i),
                                           QStringLiteral("C:/tmp"))))
            written << SessionStore::pathForId(ids.at(i));
    }

    MainWindow window;
    window.resize(560, 620);
    window.show();
    pump(300);

    QLineEdit *box = nullptr;
    for (QLineEdit *c : window.findChildren<QLineEdit *>()) {
        if (c->property("role").toString() == QStringLiteral("sessionSearch")) {
            box = c;
            break;
        }
    }
    if (!box) {
        out << "no search box\n";
        return 1;
    }

    box->setFocus();
    pump(250);

    const QString shot = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                  : QDir::current().filePath(QStringLiteral("shot_suggest.png"));
    QImage img(window.width(), window.height(), QImage::Format_ARGB32_Premultiplied);
    img.fill(QColor(QStringLiteral("#0b0d10")));
    window.render(&img, QPoint(), QRegion(), QWidget::DrawChildren);
    img.save(shot);

    for (const QString &p : written)
        QFile::remove(p);
    out << "saved " << shot << "\n";
    return 0;
}
