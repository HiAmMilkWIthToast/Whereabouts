// Writes the application icon to disk: a multi-resolution .ico for the Windows
// resource, and a PNG for anything else that wants one. Development helper only.
//
// The icon is drawn with the same code the app uses at runtime, so the file on
// disk cannot drift away from the mark in the title bar.
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

// The sizes Windows asks for: 16 and 32 in the title bar and Explorer's small
// views, 48 in the medium views, 256 for large icons and the Start menu.
static const int kSizes[] = {16, 24, 32, 48, 64, 128, 256};

// Below 32px the full mark's stones merge into one blob: their gaps are under a
// pixel at that size, and the eye reads a rounded lump rather than a stack.
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

    // The same 20% inset the window icon uses, so the file and the running app
    // show the same proportions.
    const int inset = qRound(size * 0.2);
    p.drawPixmap(inset, inset,
                 Icons::render(mark, size - inset * 2, Theme::brand(), 1.0));
    p.end();
    return pix;
}

// Builds a real multi-image ICO: a 6-byte directory header, one 16-byte entry per
// image, then the images as PNG streams.
//
// Qt's own ICO writer emits only the single image it is handed. Passing it a 256
// produced a file Windows had to downscale to 16 for the title bar, which threw
// away the compact mark entirely and left small sizes soft.
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
    // Reserved, type 1 = icon, count.
    ds << quint16(0) << quint16(1) << quint16(images.size());

    // Image data starts after the header and the whole directory.
    quint32 offset = 6 + 16 * quint32(images.size());
    for (int i = 0; i < images.size(); ++i) {
        const int w = images.at(i).width();
        const int h = images.at(i).height();
        // 256 is stored as 0: the field is one byte wide and predates images
        // that large.
        ds << quint8(w >= 256 ? 0 : w) << quint8(h >= 256 ? 0 : h)
           << quint8(0) << quint8(0)     // palette count, reserved
           << quint16(1) << quint16(32)  // planes, bits per pixel
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
