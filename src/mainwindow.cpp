#include "mainwindow.h"
#include "entryicon.h"
#include "inspector.h"
#include "layoutview.h"
#include "onboarding.h"
#include "sessionstore.h"
#include "theme.h"

#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#endif

namespace {

// A card in the library list: name, when it was taken, and what is in it.
class SessionCard : public QWidget
{
public:
    explicit SessionCard(const Session &session, QWidget *parent = nullptr)
        : QWidget(parent)
    {
        auto *root = new QHBoxLayout(this);
        root->setContentsMargins(12, 10, 12, 10);
        root->setSpacing(11);

        auto *icon = new QLabel(this);
        icon->setFixedSize(26, 26);
        icon->setPixmap(Icons::render(
            session.livePositions ? Icons::Id::Layout : Icons::Id::Grid, 24,
            session.livePositions ? Theme::accent() : QColor(QStringLiteral("#8a919b")),
            devicePixelRatioF()));
        root->addWidget(icon, 0, Qt::AlignTop);

        auto *text = new QVBoxLayout;
        text->setSpacing(2);

        auto *title = new QLabel(session.name, this);
        title->setObjectName(QStringLiteral("fieldValue"));
        title->setStyleSheet(QStringLiteral("font-size: 12px; color: #e4e7ec;"));
        text->addWidget(title);

        auto *meta = new QLabel(
            QStringLiteral("%1  ·  %2  ·  %3")
                .arg(session.capturedAt.toString(QStringLiteral("MMM d, HH:mm")),
                     session.summary(),
                     Theme::formatBytes(session.totalSize())),
            this);
        meta->setObjectName(QStringLiteral("fieldLabel"));
        text->addWidget(meta);

        auto *idLabel = new QLabel(session.id, this);
        idLabel->setObjectName(QStringLiteral("mono"));
        idLabel->setStyleSheet(QStringLiteral("color: #5f6670; font-size: 10px;"));
        text->addWidget(idLabel);

        root->addLayout(text, 1);

        auto *badge = new QLabel(session.livePositions
                                     ? QStringLiteral("live")
                                     : QStringLiteral("grid"),
                                 this);
        badge->setObjectName(session.livePositions ? QStringLiteral("chipAccent")
                                                   : QStringLiteral("chip"));
        root->addWidget(badge, 0, Qt::AlignTop);
    }
};

// Shell "show in folder" with the file selected. Falls back to opening the
// folder when the item no longer exists, which is common for old snapshots.
void revealInShell(const QString &path)
{
#ifdef Q_OS_WIN
    const std::wstring native = QDir::toNativeSeparators(path).toStdWString();
    if (QFileInfo::exists(path)) {
        PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(native.c_str());
        if (pidl) {
            SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
            ILFree(pidl);
            return;
        }
    }
    const QString folder = QFileInfo(path).absolutePath();
    ShellExecuteW(nullptr, L"open", reinterpret_cast<LPCWSTR>(
                      QDir::toNativeSeparators(folder).utf16()),
                  nullptr, nullptr, SW_SHOWNORMAL);
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : ChromeWindow(parent)
{
    setWindowTitle(QStringLiteral("Cairn"));
    resize(1040, 660);

    createTitleBar();
    auto *body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    m_sidebar = new Sidebar(this);
    body->addWidget(m_sidebar);

    m_pages = new QStackedWidget(this);
    m_pages->setObjectName(QStringLiteral("pages"));
    body->addWidget(m_pages, 1);

    frameLayout()->addLayout(body, 1);

    m_status = new StatusBar(this);
    frameLayout()->addWidget(m_status);

    m_pages->addWidget(buildLibraryPage());
    m_pages->addWidget(buildDetailPage());

    m_libraryNav = m_sidebar->addPage(Icons::Id::Layout, QStringLiteral("Sessions"));
    m_newNav = m_sidebar->addPage(Icons::Id::Plus, QStringLiteral("New snapshot"));
    m_openNav = m_sidebar->addPage(Icons::Id::Search, QStringLiteral("Open by handle"));
    m_sidebar->setActive(m_libraryNav);

    connect(m_sidebar, &Sidebar::pageRequested, this, [this](int id) {
        if (id == m_newNav) {
            newSnapshot();
            m_sidebar->setActive(m_current.id.isEmpty() ? m_libraryNav : m_openNav);
        } else if (id == m_openNav) {
            openSessionById(QString());
            m_sidebar->setActive(m_current.id.isEmpty() ? m_libraryNav : m_openNav);
        } else {
            setPage(LibraryPage);
        }
    });

    m_status->setState(QStringLiteral("ready"), true);
    m_status->setHint(QStringLiteral("Ctrl+N new  ·  Ctrl+O open a .session file"));

    refreshLibrary();
}

QWidget *MainWindow::buildLibraryPage()
{
    auto *page = new QWidget(this);
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(Metrics::kPagePad, 18, Metrics::kPagePad, 16);
    root->setSpacing(0);

    auto *head = new QHBoxLayout;
    head->setSpacing(10);

    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    auto *title = new QLabel(QStringLiteral("Sessions"), page);
    title->setObjectName(QStringLiteral("pageTitle"));
    titles->addWidget(title);

    m_libraryCount = new QLabel(QString(), page);
    m_libraryCount->setObjectName(QStringLiteral("pageSubtitle"));
    titles->addWidget(m_libraryCount);
    head->addLayout(titles, 1);

    m_search = new QLineEdit(page);
    m_search->setObjectName(QStringLiteral("field"));
    m_search->setPlaceholderText(QStringLiteral("Filter sessions"));
    m_search->setFixedWidth(190);
    m_search->setClearButtonEnabled(true);
    head->addWidget(m_search, 0, Qt::AlignVCenter);

    auto *newButton = new QPushButton(QStringLiteral("New snapshot"), page);
    newButton->setObjectName(QStringLiteral("primary"));
    newButton->setCursor(Qt::PointingHandCursor);
    head->addWidget(newButton, 0, Qt::AlignVCenter);

    root->addLayout(head);
    root->addSpacing(14);

    m_list = new QListWidget(page);
    m_list->setObjectName(QStringLiteral("sessionList"));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSpacing(0);
    m_list->setUniformItemSizes(false);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    root->addWidget(m_list, 1);

    connect(newButton, &QPushButton::clicked, this, &MainWindow::newSnapshot);
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &text) {
        for (int i = 0; i < m_list->count(); ++i) {
            QListWidgetItem *item = m_list->item(i);
            const bool match = text.isEmpty()
                || item->data(Qt::UserRole + 1).toString().contains(text, Qt::CaseInsensitive)
                || item->data(Qt::UserRole + 2).toString().contains(text, Qt::CaseInsensitive);
            item->setHidden(!match);
        }
    });
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        if (item)
            openSessionById(item->data(Qt::UserRole).toString());
    });
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (item)
            openSessionById(item->data(Qt::UserRole).toString());
    });
    connect(m_list, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QListWidgetItem *item = m_list->itemAt(pos);
        if (!item)
            return;
        showSessionMenu(item->data(Qt::UserRole).toString());
    });

    return page;
}

QWidget *MainWindow::buildDetailPage()
{
    auto *page = new QWidget(this);
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(Metrics::kPagePad, 14, Metrics::kPagePad, 14);
    root->setSpacing(0);

    // ---- header
    auto *head = new QHBoxLayout;
    head->setSpacing(10);

    auto *back = new QPushButton(QStringLiteral("Sessions"), page);
    back->setObjectName(QStringLiteral("ghost"));
    back->setCursor(Qt::PointingHandCursor);
    back->setFixedHeight(24);
    // The page title sits immediately after this button, so it needs a little
    // breathing room to read as a separate control rather than a prefix.
    head->addWidget(back);
    head->addSpacing(6);

    auto *titles = new QVBoxLayout;
    titles->setSpacing(1);
    m_detailTitle = new QLabel(page);
    m_detailTitle->setObjectName(QStringLiteral("pageTitle"));
    titles->addWidget(m_detailTitle);

    m_detailMeta = new QLabel(page);
    m_detailMeta->setObjectName(QStringLiteral("pageSubtitle"));
    titles->addWidget(m_detailMeta);
    head->addLayout(titles, 1);

    auto *search = new QLineEdit(page);
    search->setObjectName(QStringLiteral("field"));
    search->setPlaceholderText(QStringLiteral("Find an item"));
    search->setFixedWidth(180);
    search->setClearButtonEnabled(true);
    head->addWidget(search, 0, Qt::AlignVCenter);
    m_find = search;

    m_viewToggle = new QPushButton(QStringLiteral("List"), page);
    m_viewToggle->setObjectName(QStringLiteral("secondary"));
    m_viewToggle->setCursor(Qt::PointingHandCursor);
    m_viewToggle->setFixedWidth(62);
    m_viewToggle->setCheckable(true);
    head->addWidget(m_viewToggle, 0, Qt::AlignVCenter);

    auto *exportButton = new QPushButton(QStringLiteral("Export"), page);
    exportButton->setObjectName(QStringLiteral("secondary"));
    exportButton->setCursor(Qt::PointingHandCursor);
    head->addWidget(exportButton, 0, Qt::AlignVCenter);

    root->addLayout(head);
    root->addSpacing(12);

    // ---- body: map | tree | inspector
    auto *body = new QHBoxLayout;
    body->setSpacing(0);

    auto *center = new QVBoxLayout;
    center->setSpacing(0);

    auto *mapCard = new QFrame(page);
    mapCard->setObjectName(QStringLiteral("card"));
    auto *mapLayout = new QVBoxLayout(mapCard);
    mapLayout->setContentsMargins(1, 1, 1, 1);
    mapLayout->setSpacing(0);
    m_layout = new LayoutView(mapCard);
    mapLayout->addWidget(m_layout);
    center->addWidget(mapCard, 1);

    body->addLayout(center, 1);

    auto *inspectorCard = new QFrame(page);
    inspectorCard->setObjectName(QStringLiteral("card"));
    auto *inspectorLayout = new QVBoxLayout(inspectorCard);
    inspectorLayout->setContentsMargins(0, 0, 0, 0);
    m_inspector = new Inspector(inspectorCard);
    inspectorLayout->addWidget(m_inspector);
    body->addWidget(inspectorCard);

    root->addLayout(body, 1);

    connect(back, &QPushButton::clicked, this, [this] { setPage(LibraryPage); });
    connect(m_viewToggle, &QPushButton::clicked, this, [this] {
        const bool list = m_viewToggle->isChecked();
        m_viewToggle->setText(list ? QStringLiteral("Map")
                                   : QStringLiteral("List"));
        m_layout->setMode(list ? LayoutView::Mode::List
                               : LayoutView::Mode::Map);
    });
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportSession);
    connect(search, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_layout->setSearch(text);
    });

    connect(m_layout, &LayoutView::entrySelected, this, [this](const QString &path) {
        for (const Entry &entry : m_current.entries) {
            if (entry.relativePath == path) {
                m_inspector->setEntry(&entry);
                break;
            }
        }
    });
    connect(m_layout, &LayoutView::entryActivated, this, [this](const QString &path) {
        revealInShell(m_current.rootPath + QLatin1Char('/') + path);
    });

    connect(m_inspector, &Inspector::revealRequested, this, [this](const QString &name) {
        revealInShell(m_current.rootPath + QLatin1Char('/') + name);
    });
    connect(m_inspector, &Inspector::copyPathRequested, this, [](const QString &text) {
        QApplication::clipboard()->setText(text);
    });
    connect(m_inspector, &Inspector::openFolderRequested, this, [](const QString &root) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(root));
    });

    return page;
}

void MainWindow::refreshLibrary()
{
    m_sessions = SessionStore::listAll();

    m_list->clear();
    for (const Session &session : m_sessions) {
        auto *item = new QListWidgetItem(m_list);
        item->setData(Qt::UserRole, session.id);
        item->setData(Qt::UserRole + 1, session.name);
        item->setData(Qt::UserRole + 2, session.rootPath);
        item->setSizeHint(QSize(0, 74));
        m_list->setItemWidget(item, new SessionCard(session, m_list));
    }

    m_libraryCount->setText(m_sessions.isEmpty()
        ? QStringLiteral("No snapshots yet — take one to get started.")
        : QStringLiteral("%1 snapshot%2 stored in %3")
              .arg(m_sessions.size())
              .arg(m_sessions.size() == 1 ? "" : "s")
              .arg(QDir::toNativeSeparators(SessionStore::directory())));

    if (m_current.id.isEmpty())
        m_status->setCounts(QStringLiteral("%1 sessions").arg(m_sessions.size()));
}

QString MainWindow::captureFolder(const QString &path, const QString &name,
                                  bool revealInLibrary)
{
    Session session = SessionStore::capture(path);
    if (session.rootPath.isEmpty() || !QFileInfo(session.rootPath).isDir()) {
        QMessageBox::warning(this, QStringLiteral("Capture failed"),
                             QStringLiteral("That folder could not be read."));
        return {};
    }

    if (!name.trimmed().isEmpty())
        session.name = name.trimmed();

    QString error;
    if (!SessionStore::save(session, &error)) {
        QMessageBox::warning(
            this, QStringLiteral("Could not save the session"),
            QStringLiteral("Writing %1 failed: %2")
                .arg(QDir::toNativeSeparators(SessionStore::pathForId(session.id)),
                     error));
        return {};
    }

    applySession(session);
    refreshLibrary();
    if (revealInLibrary)
        setPage(DetailPage);
    m_status->setState(QStringLiteral("captured"), true);
    return session.id;
}

void MainWindow::presentSession(const Session &session)
{
    applySession(session);
    setPage(DetailPage);
}

void MainWindow::applySession(const Session &session){
    m_current = session;
    m_detailTitle->setText(session.name);
    m_detailMeta->setText(
        QStringLiteral("%1  ·  %2  ·  %3")
            .arg(session.id,
                 session.summary(),
                 session.capturedAt.toString(QStringLiteral("MMM d, yyyy  HH:mm"))));

    m_layout->setSession(session);
    m_inspector->setSession(session);
    m_inspector->setEntry(nullptr);
    if (m_find)
        m_find->clear();

    m_status->setSession(QStringLiteral("%1  ·  %2").arg(session.name, session.id));
    m_status->setCounts(QStringLiteral("%1 items  ·  %2")
                            .arg(session.entries.size())
                            .arg(Theme::formatBytes(session.totalSize())));
    m_status->setState(session.livePositions ? QStringLiteral("live layout")
                                             : QStringLiteral("grid layout"),
                       session.livePositions);
    m_sidebar->setActive(m_openNav);
}

void MainWindow::openSessionById(const QString &id)
{
    QString wanted = id;
    if (wanted.isEmpty()) {
        bool ok = false;
        wanted = QInputDialog::getText(
            this, QStringLiteral("Open a session"),
            QStringLiteral("Session handle or .session file path:"),
            QLineEdit::Normal,
            QSettings().value(QStringLiteral("sessions/lastId")).toString(), &ok);
        if (!ok || wanted.trimmed().isEmpty())
            return;
        wanted = wanted.trimmed();
    }

    // Accept a path as well as a bare handle, so a file can be pasted in.
    QString path = wanted;
    if (!QFileInfo::exists(path))
        path = SessionStore::pathForId(wanted);
    if (!QFileInfo::exists(path)) {
        QMessageBox::information(
            this, QStringLiteral("Session not found"),
            QStringLiteral("No session called \"%1\".").arg(wanted));
        return;
    }

    bool ok = false;
    const Session session = SessionStore::load(path, &ok);
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("Unreadable session"),
                             QStringLiteral("That file is not a valid .session file."));
        return;
    }

    applySession(session);
    setPage(DetailPage);
}

QString MainWindow::runSnapshotDialog(bool firstRun)
{
    OnboardingDialog dialog(SessionStore::desktopPath(), firstRun, this);
    if (dialog.exec() != QDialog::Accepted)
        return QString();

    const QString id = captureFolder(dialog.chosenPath(), dialog.sessionName());
    if (id.isEmpty())
        return QString();

    // The dialog offers to show the result in its own window; honour that by
    // handing the handle it just produced to an independent viewer.
    if (dialog.openInNewWindow())
        emit sessionOpened(id);
    return id;
}

void MainWindow::newSnapshot()
{
    runSnapshotDialog(false);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // The footer advertises these, so they have to work.
    const bool control = event->modifiers() & Qt::ControlModifier;
    if (control && event->key() == Qt::Key_N) {
        newSnapshot();
        event->accept();
        return;
    }
    if (control && event->key() == Qt::Key_O) {
        openSessionById(QString());
        event->accept();
        return;
    }
    if (control && event->key() == Qt::Key_F) {
        if (m_pages->currentIndex() == DetailPage && m_find) {
            m_find->setFocus();
            m_find->selectAll();
        } else if (m_search) {
            m_search->setFocus();
            m_search->selectAll();
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && m_pages->currentIndex() == DetailPage) {
        setPage(LibraryPage);
        event->accept();
        return;
    }
    ChromeWindow::keyPressEvent(event);
}

void MainWindow::showSessionMenu(const QString &id)
{
    QMenu menu(this);
    QAction *open = menu.addAction(QStringLiteral("Open"));
    QAction *openWindow = menu.addAction(QStringLiteral("Open in a new window"));
    QAction *copyId = menu.addAction(QStringLiteral("Copy handle"));
    menu.addSeparator();
    QAction *reveal = menu.addAction(QStringLiteral("Show the .session file"));
    QAction *remove = menu.addAction(QStringLiteral("Delete snapshot"));

    QAction *chosen = menu.exec(QCursor::pos());
    if (!chosen)
        return;

    if (chosen == open) {
        openSessionById(id);
    } else if (chosen == openWindow) {
        emit sessionOpened(id);
    } else if (chosen == copyId) {
        QApplication::clipboard()->setText(id);
        m_status->setState(QStringLiteral("handle copied"), true);
    } else if (chosen == reveal) {
        revealInShell(SessionStore::pathForId(id));
    } else if (chosen == remove) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Delete snapshot"),
            QStringLiteral("Delete \"%1\"? The folder itself is not touched — "
                           "only the recorded layout.").arg(id));
        if (answer != QMessageBox::Yes)
            return;
        QFile::remove(SessionStore::pathForId(id));
        if (m_current.id == id) {
            m_current = Session();
            m_inspector->setSession(Session());
            setPage(LibraryPage);
        }
        refreshLibrary();
    }
}

void MainWindow::exportSession()
{
    if (m_current.id.isEmpty())
        return;

    const QString target = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export session"),
        QDir::homePath() + QLatin1Char('/') + m_current.id + QStringLiteral(".session"),
        QStringLiteral("Cairn session (*.session)"));
    if (target.isEmpty())
        return;
    QFile::remove(target);
    if (!QFile::copy(SessionStore::pathForId(m_current.id), target)) {
        QMessageBox::warning(this, QStringLiteral("Export failed"),
                             QStringLiteral("The session file could not be copied."));
        return;
    }
    m_status->setState(QStringLiteral("exported"), true);
}

void MainWindow::setPage(Page page)
{
    m_pages->setCurrentIndex(int(page));
    m_sidebar->setActive(page == LibraryPage ? m_libraryNav : m_openNav);
    if (page == LibraryPage)
        m_status->setState(QStringLiteral("ready"));
}
