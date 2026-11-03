
#include <QApplication>
#include <QBuffer>
#include <QDataStream>
#include <QFile>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QTextStream>

#include "icons.h"
#include "theme.h"

static QTextStream out(stdout);

static const int kSizes[] = {16, 24, 32, 48, 64, 128, 256};

static Icons::Id markFor(int size)
{
    return size < 32 ? Icons::Id::CairnCompact : Icons::Id::Cairn;
}

static QPixmap renderAt(int size, Icons::Id mark)
{
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(QStringLiteral("#15181d")));
    p.drawRoundedRect(QRectF(0, 0, size, size), size * 0.22, size * 0.22);

    const int inset = qRound(size * 0.2);
    p.drawPixmap(inset, inset,
                 Icons::render(mark, size - inset * 2, Theme::brand(), 1.0));
    p.end();
    return pix;
}

static QByteArray writeIco(const QList<QImage> &images)
{
    if (images.isEmpty())
        return QByteArray();

    QList<QByteArray> payloads;
    for (const QImage &img : images) {
        QBuffer buf;
        buf.open(QIODevice::WriteOnly);
        if (!img.save(&buf, "PNG"))
            return QByteArray();
        payloads << buf.data();
    }

    QByteArray out;
    QDataStream ds(&out, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);

    ds << quint16(0) << quint16(1) << quint16(images.size());

    quint32 offset = 6 + 16 * quint32(images.size());
    for (int i = 0; i < images.size(); ++i) {
        const int w = images.at(i).width();
        const int h = images.at(i).height();

        ds << quint8(w >= 256 ? 0 : w) << quint8(h >= 256 ? 0 : h)
           << quint8(0) << quint8(0)
           << quint16(1) << quint16(32)
           << quint32(payloads.at(i).size()) << offset;
        offset += quint32(payloads.at(i).size());
    }
    for (const QByteArray &p : payloads)
        out.append(p);
    return out;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CairnIconTool"));
    app.setOrganizationName(QStringLiteral("CairnIconTool"));

    const QString ico = argc > 1
        ? QString::fromLocal8Bit(argv[1])
        : QStringLiteral("C:/Users/Kille/MonkeyCode/cairn-src/res/cairn.ico");
    const QString png = argc > 2
        ? QString::fromLocal8Bit(argv[2])
        : QStringLiteral("C:/Users/Kille/MonkeyCode/cairn-src/res/cairn-256.png");

    QList<QImage> images;
    for (int size : kSizes)
        images << renderAt(size, markFor(size)).toImage();

    bool ok = true;
    const QByteArray bytes = writeIco(images);
    if (bytes.isEmpty()) {
        out << "ico assembly failed\n";
        ok = false;
    } else {
        QFile f(ico);
        if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size()) {
            out << "ico write failed: " << f.errorString() << "\n";
            ok = false;
        }
    }

    if (!renderAt(256, markFor(256)).save(png, "PNG")) {
        out << "png write failed\n";
        ok = false;
    }

    out << (ok ? "wrote " : "FAILED ") << ico << " and " << png << "\n";
    out.flush();
    return ok ? 0 : 1;
}
