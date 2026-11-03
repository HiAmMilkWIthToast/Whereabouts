#include "inspector.h"
#include "icontextbutton.h"
#include "entryicon.h"
#include "theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

QLabel *makeLabel(const QString &text, const QString &objectName, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(objectName);
    label->setWordWrap(true);
    return label;
}

QFrame *makeDivider(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setObjectName(QStringLiteral("divider"));
    line->setFixedHeight(1);
    return line;
}

void clearLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->setParent(nullptr);
            delete widget;
            delete item;
        } else if (QLayout *child = item->layout()) {

            clearLayout(child);
            delete child;
        } else {
            delete item;
        }
    }
}

}

Inspector::Inspector(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(Metrics::kInspectorWidth);
    setAttribute(Qt::WA_StyledBackground, false);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    auto *header = new QHBoxLayout;
    header->setSpacing(8);

    auto *icon = new QLabel(this);
    icon->setObjectName(QStringLiteral("inspectorIcon"));
    icon->setFixedSize(16, 16);
    icon->setPixmap(Icons::render(Icons::Id::Info, 15, QColor(QStringLiteral("#7d848d")),
                                  devicePixelRatioF()));
    header->addWidget(icon);

    m_title = makeLabel(QStringLiteral("Session"), QStringLiteral("fieldValue"), this);
    header->addWidget(m_title, 1);
    root->addLayout(header);

    m_subtitle = makeLabel(QString(), QStringLiteral("fieldLabel"), this);
    root->addWidget(m_subtitle);

    root->addWidget(makeDivider(this));

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("clean"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_bodyHost = new QWidget(scroll);
    m_bodyHost->setObjectName(QStringLiteral("clean"));
    m_body = new QVBoxLayout(m_bodyHost);
    m_body->setContentsMargins(0, 0, 0, 0);
    m_body->setSpacing(8);
    scroll->setWidget(m_bodyHost);
    root->addWidget(scroll, 1);

    buildSummary(Session());
}

QLabel *Inspector::addRow(QVBoxLayout *layout, const QString &key, const QString &value,
                          bool mono)
{
    auto *row = new QVBoxLayout;
    row->setSpacing(2);

    auto *keyLabel = makeLabel(key, QStringLiteral("fieldLabel"), m_bodyHost);
    row->addWidget(keyLabel);

    auto *valueLabel = makeLabel(value, mono ? QStringLiteral("mono")
                                             : QStringLiteral("fieldValue"),
                                 m_bodyHost);
    valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    row->addWidget(valueLabel);

    layout->addLayout(row);
    return valueLabel;
}

void Inspector::clearBody()
{

    clearLayout(m_body);
}

void Inspector::setSession(const Session &session)
{
    m_session = session;
    m_hasEntry = false;
    clearBody();
    m_title->setText(session.id.isEmpty() ? QStringLiteral("No session")
                                          : session.name);
    m_subtitle->setText(session.capturedAt.isValid()
        ? QStringLiteral("captured %1").arg(
              session.capturedAt.toString(QStringLiteral("MMM d, HH:mm")))
        : QString());

    buildSummary(session);
}

void Inspector::setEntry(const Entry *entry)
{
    if (!entry) {
        m_hasEntry = false;
        clearBody();
        buildSummary(m_session);
        return;
    }

    m_entry = *entry;
    m_hasEntry = true;

    clearBody();
    buildEntry(m_session, m_entry);
}

void Inspector::buildSummary(const Session &session)
{
    if (session.id.isEmpty()) {
        m_body->addWidget(makeLabel(
            QStringLiteral("Choose a session to inspect its layout."),
            QStringLiteral("emptyHint"), m_bodyHost));
        m_body->addStretch(1);
        return;
    }

    QList<Kind> present;
    const Kind order[] = {Kind::Folder, Kind::Archive, Kind::Executable, Kind::Document,
                          Kind::Image, Kind::Code, Kind::Audio, Kind::Video,
                          Kind::Shortcut, Kind::Other};
    for (Kind kind : order) {
        if (session.countOf(kind) > 0)
            present.append(kind);
    }

    auto *rows = new QVBoxLayout;
    rows->setSpacing(3);
    for (Kind kind : present) {
        auto *line = new QHBoxLayout;
        line->setSpacing(7);

        auto *dot = new QLabel(m_bodyHost);
        dot->setFixedSize(9, 9);
        QPixmap pix(9, 9);
        pix.fill(Qt::transparent);
        {
            QPainter p(&pix);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(EntryIcon::kindColor(kind));
            p.drawEllipse(QRectF(0.5, 0.5, 8, 8));
        }
        dot->setPixmap(pix);
        line->addWidget(dot);

        line->addWidget(makeLabel(kindName(kind), QStringLiteral("fieldValue"),
                                  m_bodyHost), 1);
        line->addWidget(new QLabel(QString::number(session.countOf(kind)), m_bodyHost));
        rows->addLayout(line);
    }
    m_body->addLayout(rows);

    m_body->addWidget(makeDivider(m_bodyHost));

    const QString source = session.livePositions
        ? QStringLiteral("Real desktop positions")
        : QStringLiteral("Computed grid layout");
    addRow(m_body, QStringLiteral("LAYOUT SOURCE"), source);
    addRow(m_body, QStringLiteral("TOTAL SIZE"), Theme::formatBytes(session.totalSize()));
    addRow(m_body, QStringLiteral("ROOT"), session.rootPath, true);

    auto *reveal = new IconTextButton(Icons::Id::Folder,
                                      QStringLiteral("Open folder"), m_bodyHost);
    reveal->setObjectName(QStringLiteral("secondary"));
    connect(reveal, &IconTextButton::clicked, this, [this] {
        if (!m_session.rootPath.isEmpty())
            emit openFolderRequested(m_session.rootPath);
    });
    m_body->addWidget(reveal);

    m_body->addStretch(1);
}

void Inspector::buildEntry(const Session &session, const Entry &entry)
{
    if (!session.id.isEmpty()) {
        m_title->setText(entry.name);
        m_subtitle->setText(kindShortName(entry.kind).toUpper()
                            + QStringLiteral(" · ") + kindName(entry.kind));
    }

    auto *glyph = new QLabel(m_bodyHost);
    glyph->setFixedSize(28, 28);
    glyph->setPixmap(Icons::render(EntryIcon::kindIcon(entry.kind), 26,
                                   EntryIcon::kindColor(entry.kind),
                                   devicePixelRatioF()));
    m_body->addWidget(glyph);

    addRow(m_body, QStringLiteral("NAME"), entry.name);
    addRow(m_body, QStringLiteral("TYPE"), kindName(entry.kind)
           + (entry.suffix.isEmpty() ? QString()
                                     : QStringLiteral("  ·  ")
                                       + entry.suffix.toUpper()));

    if (entry.kind == Kind::Folder) {
        addRow(m_body, QStringLiteral("CONTAINS"),
               entry.children >= 0 ? QStringLiteral("%1 items").arg(entry.children)
                                   : QStringLiteral("—"));
    } else {
        addRow(m_body, QStringLiteral("SIZE"), Theme::formatBytes(entry.size));
    }

    if (entry.modified.isValid()) {
        addRow(m_body, QStringLiteral("MODIFIED"),
               entry.modified.toString(QStringLiteral("yyyy-MM-dd  HH:mm")));
    }

    addRow(m_body, QStringLiteral("POSITION"),
           entry.fromShell ? QStringLiteral("%1, %2   (live)")
                             .arg(entry.pos.x()).arg(entry.pos.y())
                           : QStringLiteral("column %1, row %2")
                             .arg(entry.cell.x() + 1).arg(entry.cell.y() + 1));

    if (!entry.target.isEmpty())
        addRow(m_body, QStringLiteral("TARGET"), entry.target, true);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(6);

    auto *reveal = new IconTextButton(Icons::Id::Folder,
                                      QStringLiteral("Reveal"), m_bodyHost);
    reveal->setObjectName(QStringLiteral("secondary"));
    connect(reveal, &IconTextButton::clicked, this, [this, entry] {
        emit revealRequested(entry.name);
    });
    buttons->addWidget(reveal);

    auto *copy = new IconTextButton(Icons::Id::Link,
                                    QStringLiteral("Copy path"), m_bodyHost);
    copy->setObjectName(QStringLiteral("secondary"));
    connect(copy, &IconTextButton::clicked, this, [this, session, entry] {
        emit copyPathRequested(session.rootPath + QLatin1Char('/') + entry.relativePath);
    });
    buttons->addWidget(copy);

    m_body->addLayout(buttons);
    m_body->addStretch(1);
}
