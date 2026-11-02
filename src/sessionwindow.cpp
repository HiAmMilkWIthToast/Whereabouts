#include "sessionwindow.h"
#include "inspector.h"
#include "layoutview.h"
#include "sessionstore.h"
#include "theme.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QUrl>
#include <QVBoxLayout>

SessionWindow::SessionWindow(QWidget *parent)
    : ChromeWindow(parent)
{
    setWindowTitle(QStringLiteral("Cairn — session"));
    resize(900, 600);

    auto *title = createTitleBar();
    m_status = new StatusBar(this);

    auto *body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    auto *center = new QVBoxLayout;
    center->setContentsMargins(Metrics::kPagePad, 14, Metrics::kPagePad, 14);
    center->setSpacing(0);

    auto *head = new QHBoxLayout;
    head->setSpacing(10);

    auto *titles = new QVBoxLayout;
    titles->setSpacing(1);
    m_title = new QLabel(QStringLiteral("No session"), this);
    m_title->setObjectName(QStringLiteral("pageTitle"));
    titles->addWidget(m_title);

    m_meta = new QLabel(this);
    m_meta->setObjectName(QStringLiteral("pageSubtitle"));
    titles->addWidget(m_meta);
    head->addLayout(titles, 1);

    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("field"));
    m_search->setPlaceholderText(QStringLiteral("Find an item"));
    m_search->setFixedWidth(180);
    m_search->setClearButtonEnabled(true);
    head->addWidget(m_search, 0, Qt::AlignVCenter);

    center->addLayout(head);
    center->addSpacing(12);

    auto *mapCard = new QFrame(this);
    mapCard->setObjectName(QStringLiteral("card"));
    auto *mapLayout = new QVBoxLayout(mapCard);
    mapLayout->setContentsMargins(1, 1, 1, 1);
    m_layout = new LayoutView(mapCard);
    mapLayout->addWidget(m_layout);
    center->addWidget(mapCard, 1);

    body->addLayout(center, 1);

    auto *inspectorCard = new QFrame(this);
    inspectorCard->setObjectName(QStringLiteral("card"));
    auto *inspectorLayout = new QVBoxLayout(inspectorCard);
    inspectorLayout->setContentsMargins(0, 0, 0, 0);
    m_inspector = new Inspector(inspectorCard);
    inspectorLayout->addWidget(m_inspector);
    body->addWidget(inspectorCard);

    frameLayout()->addLayout(body, 1);
    frameLayout()->addWidget(m_status);

    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_layout->setSearch(text);
    });
    connect(m_layout, &LayoutView::entrySelected, this, [this](const QString &path) {
        for (const Entry &entry : m_session.entries) {
            if (entry.relativePath == path) {
                m_inspector->setEntry(&entry);
                return;
            }
        }
        m_inspector->setEntry(nullptr);
    });
    connect(m_inspector, &Inspector::openFolderRequested, this, [](const QString &root) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(root));
    });
    connect(m_inspector, &Inspector::revealRequested, this, [this](const QString &name) {
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(m_session.rootPath + QLatin1Char('/') + name));
    });
    connect(m_inspector, &Inspector::copyPathRequested, this, [](const QString &text) {
        QApplication::clipboard()->setText(text);
    });

    title->setSubtitle(QStringLiteral("session viewer"));
    m_status->setState(QStringLiteral("no session"));
    m_status->setHint(QStringLiteral("Ctrl+F find  ·  Ctrl+0 fit"));
}

bool SessionWindow::showSession(const QString &idOrPath){
    QString path = idOrPath;
    if (!QFileInfo::exists(path))
        path = SessionStore::pathForId(idOrPath);

    bool ok = false;
    const Session session = SessionStore::load(path, &ok);
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("Unreadable session"),
                             QStringLiteral("\"%1\" is not a valid .session file.")
                                 .arg(idOrPath));
        return false;
    }

    m_session = session;
    setWindowTitle(QStringLiteral("Cairn — %1").arg(session.id));
    m_title->setText(session.name);
    m_meta->setText(
        QStringLiteral("%1  ·  %2  ·  %3")
            .arg(session.id,
                 session.summary(),
                 session.capturedAt.toString(QStringLiteral("MMM d, yyyy  HH:mm"))));

    m_layout->setSession(session);
    m_inspector->setSession(session);
    m_inspector->setEntry(nullptr);

    m_status->setState(session.livePositions ? QStringLiteral("live layout")
                                             : QStringLiteral("grid layout"),
                       session.livePositions);
    m_status->setSession(QStringLiteral("%1 items").arg(session.entries.size()));
    m_status->setCounts(Theme::formatBytes(session.totalSize()));
    show();
    return true;
}

void SessionWindow::keyPressEvent(QKeyEvent *event)
{
    // The footer advertises these, so they have to work.
    const bool control = event->modifiers() & Qt::ControlModifier;
    if (control && event->key() == Qt::Key_F) {
        m_search->setFocus();
        m_search->selectAll();
        event->accept();
        return;
    }
    if (control && (event->key() == Qt::Key_0 || event->key() == Qt::Key_Equal)) {
        m_layout->fitToView();
        event->accept();
        return;
    }
    ChromeWindow::keyPressEvent(event);
}
