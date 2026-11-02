#include "sessionmodel.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QSet>

namespace {

const QSet<QString> &archiveSuffixes()
{
    static const QSet<QString> set = {
        QStringLiteral("zip"),  QStringLiteral("rar"),  QStringLiteral("7z"),
        QStringLiteral("tar"),  QStringLiteral("gz"),   QStringLiteral("tgz"),
        QStringLiteral("bz2"),  QStringLiteral("xz"),   QStringLiteral("cab"),
        QStringLiteral("iso"),  QStringLiteral("jar"),  QStringLiteral("apk"),
        QStringLiteral("war"),  QStringLiteral("lz"),   QStringLiteral("zst"),
        QStringLiteral("zipx"), QStringLiteral("lzma")
    };
    return set;
}

const QSet<QString> &executableSuffixes()
{
    static const QSet<QString> set = {
        QStringLiteral("exe"), QStringLiteral("msi"), QStringLiteral("bat"),
        QStringLiteral("cmd"), QStringLiteral("com"), QStringLiteral("scr"),
        QStringLiteral("ps1"), QStringLiteral("vbs"), QStringLiteral("appx"),
        QStringLiteral("msix"), QStringLiteral("jar_")
    };
    return set;
}

const QSet<QString> &imageSuffixes()
{
    static const QSet<QString> set = {
        QStringLiteral("png"),  QStringLiteral("jpg"), QStringLiteral("jpeg"),
        QStringLiteral("gif"),  QStringLiteral("bmp"), QStringLiteral("webp"),
        QStringLiteral("svg"),  QStringLiteral("ico"), QStringLiteral("tif"),
        QStringLiteral("tiff"), QStringLiteral("heic"), QStringLiteral("avif")
    };
    return set;
}

const QSet<QString> &documentSuffixes()
{
    static const QSet<QString> set = {
        QStringLiteral("txt"),  QStringLiteral("pdf"), QStringLiteral("doc"),
        QStringLiteral("docx"), QStringLiteral("rtf"), QStringLiteral("odt"),
        QStringLiteral("xls"),  QStringLiteral("xlsx"), QStringLiteral("csv"),
        QStringLiteral("ppt"),  QStringLiteral("pptx"), QStringLiteral("md"),
        QStringLiteral("log"),  QStringLiteral("nfo"), QStringLiteral("ini")
    };
    return set;
}

const QSet<QString> &codeSuffixes()
{
    static const QSet<QString> set = {
        QStringLiteral("c"),   QStringLiteral("h"),    QStringLiteral("cpp"),
        QStringLiteral("hpp"), QStringLiteral("cc"),   QStringLiteral("cs"),
        QStringLiteral("py"),  QStringLiteral("js"),   QStringLiteral("ts"),
        QStringLiteral("tsx"), QStringLiteral("jsx"),  QStringLiteral("json"),
        QStringLiteral("xml"), QStringLiteral("html"), QStringLiteral("css"),
        QStringLiteral("scss"),QStringLiteral("sh"),   QStringLiteral("go"),
        QStringLiteral("rs"),  QStringLiteral("java"), QStringLiteral("rb"),
        QStringLiteral("php"), QStringLiteral("lua"),  QStringLiteral("yml"),
        QStringLiteral("yaml"),QStringLiteral("toml"), QStringLiteral("sql"),
        QStringLiteral("cmake"), QStringLiteral("qml"), QStringLiteral("bat_")
    };
    return set;
}

const QSet<QString> &audioSuffixes()
{
    static const QSet<QString> set = {
        QStringLiteral("mp3"), QStringLiteral("wav"), QStringLiteral("flac"),
        QStringLiteral("ogg"), QStringLiteral("m4a"), QStringLiteral("aac"),
        QStringLiteral("wma")
    };
    return set;
}

const QSet<QString> &videoSuffixes()
{
    static const QSet<QString> set = {
        QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"),
        QStringLiteral("mov"), QStringLiteral("wmv"), QStringLiteral("webm"),
        QStringLiteral("flv"), QStringLiteral("m4v")
    };
    return set;
}

// Reads the first handful of bytes. Returns an empty array when the file cannot
// be opened, which simply means "trust the extension".
QByteArray headOf(const QString &path, int bytes = 8)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.read(bytes);
}

bool looksLikeArchive(const QByteArray &head)
{
    if (head.startsWith("PK"))                 // zip, jar, apk, docx, ...
        return true;
    if (head.startsWith("Rar!"))
        return true;
    if (head.startsWith("7z\xBC\xAF\x27\x1C"))
        return true;
    if (head.startsWith("\x1F\x8B"))           // gzip
        return true;
    if (head.startsWith("BZh"))
        return true;
    if (head.startsWith("\xFD" "7zXZ"))        // xz
        return true;
    return false;
}

bool looksLikeExecutable(const QByteArray &head)
{
    // PE images start with "MZ"; the plain-text script types are already
    // decided by their extension.
    return head.startsWith("MZ");
}

} // namespace

QString kindName(Kind kind)
{
    switch (kind) {
    case Kind::Folder:     return QStringLiteral("Folder");
    case Kind::Archive:    return QStringLiteral("Archive");
    case Kind::Executable: return QStringLiteral("Executable");
    case Kind::Document:   return QStringLiteral("Document");
    case Kind::Image:      return QStringLiteral("Image");
    case Kind::Code:       return QStringLiteral("Source");
    case Kind::Audio:      return QStringLiteral("Audio");
    case Kind::Video:      return QStringLiteral("Video");
    case Kind::Shortcut:   return QStringLiteral("Shortcut");
    case Kind::Other:      return QStringLiteral("File");
    }
    return QStringLiteral("File");
}

QString kindShortName(Kind kind)
{
    switch (kind) {
    case Kind::Folder:     return QStringLiteral("dir");
    case Kind::Archive:    return QStringLiteral("zip");
    case Kind::Executable: return QStringLiteral("exe");
    case Kind::Document:   return QStringLiteral("doc");
    case Kind::Image:      return QStringLiteral("img");
    case Kind::Code:       return QStringLiteral("src");
    case Kind::Audio:      return QStringLiteral("aud");
    case Kind::Video:      return QStringLiteral("vid");
    case Kind::Shortcut:   return QStringLiteral("lnk");
    case Kind::Other:      return QStringLiteral("file");
    }
    return QStringLiteral("file");
}

QString kindColor(Kind kind)
{
    // Restrained, slightly desaturated hues that sit on near-black surfaces.
    switch (kind) {
    case Kind::Folder:     return QStringLiteral("#8fa4bf");
    case Kind::Archive:    return QStringLiteral("#d0a86a");
    case Kind::Executable: return QStringLiteral("#c98b8b");
    case Kind::Document:   return QStringLiteral("#9fb0a4");
    case Kind::Image:      return QStringLiteral("#a892c4");
    case Kind::Code:       return QStringLiteral("#7fb0c9");
    case Kind::Audio:      return QStringLiteral("#b09ac9");
    case Kind::Video:      return QStringLiteral("#c49a9a");
    case Kind::Shortcut:   return QStringLiteral("#8a919b");
    case Kind::Other:      return QStringLiteral("#7d848d");
    }
    return QStringLiteral("#7d848d");
}

namespace KindDetect {

Kind kindForSuffix(const QString &suffix)
{
    if (suffix.isEmpty())
        return Kind::Other;
    if (suffix == QLatin1String("lnk") || suffix == QLatin1String("url"))
        return Kind::Shortcut;
    if (archiveSuffixes().contains(suffix))
        return Kind::Archive;
    if (executableSuffixes().contains(suffix))
        return Kind::Executable;
    if (imageSuffixes().contains(suffix))
        return Kind::Image;
    if (documentSuffixes().contains(suffix))
        return Kind::Document;
    if (codeSuffixes().contains(suffix))
        return Kind::Code;
    if (audioSuffixes().contains(suffix))
        return Kind::Audio;
    if (videoSuffixes().contains(suffix))
        return Kind::Video;
    return Kind::Other;
}

Kind classify(const QString &path, const QString &suffix, bool isDir)
{
    if (isDir)
        return Kind::Folder;

    const Kind byName = kindForSuffix(suffix);

    // A renamed file is common on a desktop (.zip that is really a text file, or
    // an .exe with no extension). Only the ambiguous cases cost a disk read.
    if (byName == Kind::Archive || byName == Kind::Executable) {
        const QByteArray head = headOf(path);
        if (!head.isEmpty()) {
            if (looksLikeExecutable(head))
                return Kind::Executable;
            if (looksLikeArchive(head))
                return Kind::Archive;
        }
        return byName;
    }

    if (byName == Kind::Shortcut)
        return Kind::Shortcut;

    if (byName == Kind::Other) {
        // No useful extension: sniff for the two kinds worth naming.
        const QByteArray head = headOf(path);
        if (looksLikeExecutable(head))
            return Kind::Executable;
        if (looksLikeArchive(head))
            return Kind::Archive;
    }

    return byName;
}

} // namespace KindDetect

QJsonObject Entry::toJson() const
{
    QJsonObject object;
    object[QStringLiteral("name")] = name;
    object[QStringLiteral("path")] = relativePath;
    object[QStringLiteral("kind")] = kindShortName(kind);
    object[QStringLiteral("suffix")] = suffix;
    object[QStringLiteral("size")] = double(size);
    object[QStringLiteral("modified")] = modified.toString(Qt::ISODate);
    if (children >= 0)
        object[QStringLiteral("children")] = children;
    if (!fromShell) {
        QJsonObject cellPos;
        cellPos[QStringLiteral("x")] = cell.x();
        cellPos[QStringLiteral("y")] = cell.y();
        object[QStringLiteral("cell")] = cellPos;
    }
    QJsonObject pixelPos;
    pixelPos[QStringLiteral("x")] = pos.x();
    pixelPos[QStringLiteral("y")] = pos.y();
    object[QStringLiteral("pos")] = pixelPos;
    if (!target.isEmpty())
        object[QStringLiteral("target")] = target;
    return object;
}

Entry Entry::fromJson(const QJsonObject &object)
{
    Entry entry;
    entry.name = object.value(QStringLiteral("name")).toString();
    entry.relativePath = object.value(QStringLiteral("path")).toString();
    entry.suffix = object.value(QStringLiteral("suffix")).toString();
    entry.size = qint64(object.value(QStringLiteral("size")).toDouble());
    entry.modified = QDateTime::fromString(
        object.value(QStringLiteral("modified")).toString(), Qt::ISODate);
    entry.target = object.value(QStringLiteral("target")).toString();

    const QString shortName = object.value(QStringLiteral("kind")).toString();
    entry.kind = shortName == QLatin1String("dir")  ? Kind::Folder
               : shortName == QLatin1String("zip")  ? Kind::Archive
               : shortName == QLatin1String("exe")  ? Kind::Executable
               : shortName == QLatin1String("doc")  ? Kind::Document
               : shortName == QLatin1String("img")  ? Kind::Image
               : shortName == QLatin1String("src")  ? Kind::Code
               : shortName == QLatin1String("aud")  ? Kind::Audio
               : shortName == QLatin1String("vid")  ? Kind::Video
               : shortName == QLatin1String("lnk")  ? Kind::Shortcut
                                                    : Kind::Other;

    if (object.contains(QStringLiteral("children")))
        entry.children = object.value(QStringLiteral("children")).toInt();

    const QJsonObject pos = object.value(QStringLiteral("pos")).toObject();
    entry.pos = QPoint(pos.value(QStringLiteral("x")).toInt(),
                       pos.value(QStringLiteral("y")).toInt());

    if (object.contains(QStringLiteral("cell"))) {
        const QJsonObject cell = object.value(QStringLiteral("cell")).toObject();
        entry.cell = QPoint(cell.value(QStringLiteral("x")).toInt(),
                            cell.value(QStringLiteral("y")).toInt());
        entry.fromShell = false;
    } else {
        // Written by a live capture: the pixel position is the real one.
        entry.cell = QPoint(-1, -1);
        entry.fromShell = true;
    }
    return entry;
}

int Session::countOf(Kind kind) const
{
    int total = 0;
    for (const Entry &entry : entries) {
        if (entry.kind == kind)
            ++total;
    }
    return total;
}

qint64 Session::totalSize() const
{
    qint64 total = 0;
    for (const Entry &entry : entries) {
        if (entry.kind != Kind::Folder)
            total += entry.size;
    }
    return total;
}

QString Session::summary() const
{
    const int folders = countOf(Kind::Folder);
    const int archives = countOf(Kind::Archive);
    const int executables = countOf(Kind::Executable);
    const int files = entries.size() - folders;

    QStringList parts;
    parts << QStringLiteral("%1 folder%2").arg(folders).arg(folders == 1 ? "" : "s");
    parts << QStringLiteral("%1 file%2").arg(files).arg(files == 1 ? "" : "s");
    if (archives > 0)
        parts << QStringLiteral("%1 archive%2").arg(archives).arg(archives == 1 ? "" : "s");
    if (executables > 0)
        parts << QStringLiteral("%1 exe%2").arg(executables).arg(executables == 1 ? "" : "s");
    return parts.join(QStringLiteral(" · "));
}

QJsonObject Session::toJson() const
{
    QJsonObject object;
    object[QStringLiteral("format")] = QStringLiteral("cairn.session");
    object[QStringLiteral("version")] = 1;
    object[QStringLiteral("id")] = id;
    object[QStringLiteral("name")] = name;
    object[QStringLiteral("root")] = rootPath;
    object[QStringLiteral("captured")] = capturedAt.toString(Qt::ISODate);
    object[QStringLiteral("livePositions")] = livePositions;

    QJsonObject screenObject;
    screenObject[QStringLiteral("w")] = screen.width();
    screenObject[QStringLiteral("h")] = screen.height();
    object[QStringLiteral("screen")] = screenObject;

    QJsonObject cellObject;
    cellObject[QStringLiteral("w")] = cellSize.width();
    cellObject[QStringLiteral("h")] = cellSize.height();
    object[QStringLiteral("cell")] = cellObject;

    QJsonArray list;
    for (const Entry &entry : entries)
        list.append(entry.toJson());
    object[QStringLiteral("entries")] = list;
    return object;
}

Session Session::fromJson(const QJsonObject &object)
{
    Session session;
    session.id = object.value(QStringLiteral("id")).toString();
    session.name = object.value(QStringLiteral("name")).toString();
    session.rootPath = object.value(QStringLiteral("root")).toString();
    session.capturedAt = QDateTime::fromString(
        object.value(QStringLiteral("captured")).toString(), Qt::ISODate);
    session.livePositions = object.value(QStringLiteral("livePositions")).toBool();

    const QJsonObject screen = object.value(QStringLiteral("screen")).toObject();
    session.screen = QSize(screen.value(QStringLiteral("w")).toInt(),
                           screen.value(QStringLiteral("h")).toInt());

    const QJsonObject cell = object.value(QStringLiteral("cell")).toObject();
    session.cellSize = QSize(cell.value(QStringLiteral("w")).toInt(),
                             cell.value(QStringLiteral("h")).toInt());

    const QJsonArray list = object.value(QStringLiteral("entries")).toArray();
    session.entries.reserve(list.size());
    for (const QJsonValue &value : list)
        session.entries.append(Entry::fromJson(value.toObject()));

    return session;
}
