
#include <QDir>
#include <QApplication>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QUrl>
#include <QWidget>

#include "chrome.h"
#include "theme.h"

static QTextStream out(stdout);
static int failures = 0;

static void check(bool ok, const QString &what)
{
    out << (ok ? "  PASS  " : "  FAIL  ") << what << "\n";
    if (!ok)
        ++failures;
}

static void pump(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

static void sendEnter(QWidget *w)
{
    QEnterEvent e(QPointF(4, 4), w->mapToGlobal(QPointF(4, 4)),
                  w->mapToGlobal(QPointF(4, 4)));
    QApplication::sendEvent(w, &e);
}

static void sendLeave(QWidget *w)
{
    QEvent e(QEvent::Leave);
    QApplication::sendEvent(w, &e);
}

static QWidget *findBrand(QWidget *root)
{
    return root->findChild<QWidget *>(QStringLiteral("appBrand"));
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setStyleSheet(Theme::buildStyleSheet());

    ChromeWindow window;
    TitleBar *bar = window.createTitleBar();
    window.resize(900, 600);
    window.show();
    pump(120);

    QWidget *brand = findBrand(bar);
    check(brand != nullptr, QStringLiteral("the brand target exists"));
    if (!brand) {
        out << "\nBRANDCHECK FAILED\n";
        out.flush();
        return 1;
    }

    out << "\n== it is a single target, not two labels ==\n";
    {

        check(brand->rect().width() > 60,
              QStringLiteral("the target covers the mark and the name (%1px wide)")
                  .arg(brand->rect().width()));
        check(brand->cursor().shape() == Qt::PointingHandCursor,
              QStringLiteral("it shows a hand cursor"));
        check(!brand->toolTip().isEmpty(),
              QStringLiteral("it explains itself: \"%1\"").arg(brand->toolTip()));
    }

    out << "\n== it is reachable and looks like a target ==\n";
    {

        check(bar->rect().contains(brand->geometry().center()),
              QStringLiteral("it sits inside the title bar"));
        check(brand->geometry().left() < bar->rect().width() / 2,
              QStringLiteral("it stays on the left, clear of the window controls"));
    }

    out << "\n== hovering animates, and settles ==\n";
    {

        sendLeave(brand);
        pump(260);
        const QImage rest = brand->grab().toImage();

        sendEnter(brand);

        pump(45);
        const QImage mid = brand->grab().toImage();
        pump(300);
        const QImage hovered = brand->grab().toImage();

        check(rest != hovered, QStringLiteral("hovering changes how it looks"));
        check(mid != rest && mid != hovered,
              QStringLiteral("the change is gradual, not a snap"));

        sendLeave(brand);
        pump(300);
        const QImage after = brand->grab().toImage();
        check(after == rest, QStringLiteral("leaving restores the resting look"));
    }

    out << "\n== pressing responds before the click lands ==\n";
    {
        sendEnter(brand);
        pump(220);
        const QImage hovered = brand->grab().toImage();

        QMouseEvent press(QEvent::MouseButtonPress, brand->rect().center(),
                          brand->mapToGlobal(brand->rect().center()),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(brand, &press);
        pump(140);
        const QImage pressed = brand->grab().toImage();
        check(pressed != hovered, QStringLiteral("pressing is visibly different"));

        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(-40, -40),
                            brand->mapToGlobal(QPointF(-40, -40)),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(brand, &release);
        pump(300);
        check(brand->grab().toImage() != pressed,
              QStringLiteral("releasing clears the pressed look"));
    }

    out << "\n== the address is the project, spelled out ==\n";
    {

        const QString expected = QStringLiteral("https://github.com/HiAmMilkWIthToast/Whereabouts");
        const QUrl url(expected);
        check(url.isValid() && url.scheme() == QLatin1String("https"),
              QStringLiteral("the address parses as an https URL"));
        check(url.host() == QLatin1String("github.com"),
              QStringLiteral("it points at github.com"));
        check(url.path().endsWith(QLatin1String("/Whereabouts")),
              QStringLiteral("and at this repository (%1)").arg(url.path()));

        QFile src(QStringLiteral(CAIRN_SRC_DIR "/chrome.cpp"));
        if (src.open(QIODevice::ReadOnly)) {
            const QString text = QString::fromUtf8(src.readAll());
            check(text.contains(expected),
                  QStringLiteral("chrome.cpp carries exactly that address"));
        } else {
            check(false, QStringLiteral("could read chrome.cpp"));
        }
    }

    out << "\n";

    {
        sendLeave(brand);
        pump(300);
        const QImage rest = bar->grab().toImage();
        sendEnter(brand);
        pump(300);
        const QImage hovered = bar->grab().toImage();
        QMouseEvent press(QEvent::MouseButtonPress, brand->rect().center(),
                          brand->mapToGlobal(brand->rect().center()),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(brand, &press);
        pump(200);
        const QImage pressed = bar->grab().toImage();

        const QRect crop(0, 0, brand->geometry().right() + 18, bar->height());
        const int s = 3;
        QImage stripImg((crop.width() * 3 + 40) * s, crop.height() * s + 20,
                        QImage::Format_ARGB32);
        stripImg.fill(QColor(QStringLiteral("#101216")));
        QPainter sp(&stripImg);
        int x = 10 * s;
        for (const QImage &im : {rest, hovered, pressed}) {
            sp.drawImage(QRectF(x, 10 * s, crop.width() * s, crop.height() * s),
                         im, QRectF(crop));
            x += (crop.width() + 10) * s;
        }
        sp.end();
        stripImg.save(QDir::current().filePath(QStringLiteral("brand_states.png")));
    }

    out << (failures == 0 ? "BRANDCHECK PASSED\n" : "BRANDCHECK FAILED\n");
    out.flush();
    return failures == 0 ? 0 : 1;
}
