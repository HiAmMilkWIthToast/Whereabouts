#include "mainwindow.h"
#include "elidedlabel.h"
#include "entryicon.h"
#include "filterchip.h"
#include "icontextbutton.h"
#include "folderwatch.h"
#include "inspector.h"
#include "layoutview.h"
#include "onboarding.h"
#include "sessionstore.h"
#include "systeminfo.h"
#include "theme.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QScreen>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QTime>
#include <QUrl>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#endif

namespace {

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

}

MainWindow::MainWindow(QWidget *parent)
    : ChromeWindow(parent)
{
    setWindowTitle(QStringLiteral("Cairn"));
    resize(1040, 660);

    const QByteArray saved = QSettings()
        .value(QStringLiteral("window/libraryGeometry")).toByteArray();
    if (!saved.isEmpty()) {
        restoreGeometry(saved);
        if (!geometryIsOnScreen()) {
            resize(1040, 660);
            move(80, 80);
        }
    }

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
        } else if (id == m_openNav) {
            openSessionById(QString());
        } else {
            setPage(LibraryPage);
        }
    });

    m_watcher = new FolderWatcher(this);
    connect(m_watcher, &FolderWatcher::folderChanged, this,
            &MainWindow::onFolderChanged);

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

    auto *count = new ElidedLabel(page);

    count->setObjectName(QStringLiteral("librarySubtitle"));
    m_libraryCount = count;
    titles->addWidget(m_libraryCount);

    m_welcome = new QLabel(page);
    m_welcome->setObjectName(QStringLiteral("welcomeLine"));
    const QString user = SystemInfo::userName();
    const QString greeting = SystemInfo::greetingFor(QTime::currentTime());
    m_welcome->setText(user.isEmpty()
                           ? greeting + QStringLiteral(".")
                           : greeting + QStringLiteral(", ") + user + QStringLiteral("."));
    titles->addWidget(m_welcome);

    head->addLayout(titles, 1);

    m_search = new QLineEdit(page);
    m_search->setObjectName(QStringLiteral("field"));

    m_search->setProperty("role", QStringLiteral("sessionSearch"));
    m_search->setPlaceholderText(QStringLiteral("Filter sessions"));
    m_search->setFixedWidth(190);
    m_search->setClearButtonEnabled(true);
    head->addWidget(m_search, 0, Qt::AlignVCenter);

    m_suggest = new QListView(page);
    m_suggest->setObjectName(QStringLiteral("suggestionList"));
    m_suggest->hide();
    m_suggest->setFrameShape(QFrame::NoFrame);
    m_suggest->setUniformItemSizes(true);
    m_suggest->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_suggest->setSelectionMode(QAbstractItemView::SingleSelection);
    m_suggest->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_suggest->setFocusPolicy(Qt::NoFocus);
    m_suggestModel = new QStandardItemModel(m_suggest);
    m_suggest->setModel(m_suggestModel);
    m_search->installEventFilter(this);

    auto *newButton = new IconTextButton(Icons::Id::Plus,
                                         QStringLiteral("New snapshot"), page);
    newButton->setObjectName(QStringLiteral("primary"));
    newButton->setToolTip(QStringLiteral("Record this folder as it is now (Ctrl+N)"));
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
        syncSearchSuggestions();

        if (!text.isEmpty())
            showSearchSuggestions();
        else
            hideSearchSuggestions();
    });
    connect(m_suggest, &QListView::clicked, this, [this](const QModelIndex &index) {
        activateSuggestion(index.row());
    });

    connect(m_suggest, &QListView::activated, this, [this](const QModelIndex &index) {
        activateSuggestion(index.row());
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

    const int kBar = Metrics::kBarControlHeight;

    auto *headGrid = new QGridLayout(page);
    headGrid->setContentsMargins(0, 0, 0, 0);
    headGrid->setHorizontalSpacing(12);
    headGrid->setVerticalSpacing(1);

    headGrid->setColumnStretch(0, 1);

    m_detailTitle = new QLabel(page);
    m_detailTitle->setObjectName(QStringLiteral("pageTitle"));
    headGrid->addWidget(m_detailTitle, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);

    m_detailMeta = new QLabel(page);
    m_detailMeta->setObjectName(QStringLiteral("pageSubtitle"));
    headGrid->addWidget(m_detailMeta, 1, 0, Qt::AlignLeft | Qt::AlignVCenter);

    auto *actions = new QHBoxLayout;
    actions->setContentsMargins(0, 0, 0, 0);
    actions->setSpacing(6);

    auto *back = new IconTextButton(Icons::Id::Home,
                                    QStringLiteral("Sessions"), page);
    back->setObjectName(QStringLiteral("backLink"));
    back->setToolTip(QStringLiteral("Back to every snapshot (Esc)"));
    back->setFixedHeight(kBar);
    actions->addWidget(back, 0, Qt::AlignVCenter);

    auto *search = new QLineEdit(page);
    search->setObjectName(QStringLiteral("field"));
    search->setPlaceholderText(QStringLiteral("Find an item"));
    search->setFixedWidth(190);
    search->setFixedHeight(kBar);
    search->setClearButtonEnabled(true);
    actions->addWidget(search, 0, Qt::AlignVCenter);
    m_find = search;

    m_viewToggle = new IconTextButton(Icons::Id::List,
                                      QStringLiteral("List"), page);
    m_viewToggle->setObjectName(QStringLiteral("secondary"));
    m_viewToggle->setFixedWidth(76);
    m_viewToggle->setFixedHeight(kBar);
    m_viewToggle->setCheckable(true);
    m_viewToggle->setToolTip(QStringLiteral("Switch between the map and a sorted list"));
    actions->addWidget(m_viewToggle, 0, Qt::AlignVCenter);

    auto *exportButton = new IconTextButton(Icons::Id::Export,
                                            QStringLiteral("Export"), page);
    exportButton->setObjectName(QStringLiteral("secondary"));
    exportButton->setFixedHeight(kBar);
    exportButton->setToolTip(
        QStringLiteral("Write this snapshot out as a .session file"));
    actions->addWidget(exportButton, 0, Qt::AlignVCenter);

    headGrid->addLayout(actions, 0, 1, Qt::AlignRight | Qt::AlignVCenter);

    root->addLayout(headGrid);

    m_kindBar = new QWidget(page);
    m_kindRow = new QHBoxLayout(m_kindBar);
    m_kindRow->setContentsMargins(0, 8, 0, 0);

    m_kindRow->setSpacing(6);
    root->addWidget(m_kindBar);

    m_sortBar = new QWidget(page);
    auto *sortRow = new QHBoxLayout(m_sortBar);
    sortRow->setContentsMargins(0, 6, 0, 0);
    sortRow->setSpacing(6);

    auto *sortLabel = new QLabel(QStringLiteral("SORT"), m_sortBar);
    sortLabel->setObjectName(QStringLiteral("fieldLabel"));
    sortRow->addWidget(sortLabel);

    m_sortBox = new QComboBox(m_sortBar);
    m_sortBox->setObjectName(QStringLiteral("field"));
    m_sortBox->addItem(QStringLiteral("Name"), int(LayoutView::Sort::Name));
    m_sortBox->addItem(QStringLiteral("Size"), int(LayoutView::Sort::Size));
    m_sortBox->addItem(QStringLiteral("Modified"), int(LayoutView::Sort::Modified));
    m_sortBox->addItem(QStringLiteral("Kind"), int(LayoutView::Sort::Kind));
    m_sortBox->setFixedWidth(110);
    m_sortBox->setFocusPolicy(Qt::NoFocus);
    sortRow->addWidget(m_sortBox);

    m_sortDir = new IconTextButton(Icons::Id::SortAsc,
                                   QStringLiteral("Ascending"), m_sortBar);
    m_sortDir->setObjectName(QStringLiteral("secondary"));
    m_sortDir->setCheckable(true);
    m_sortDir->setFocusPolicy(Qt::NoFocus);
    m_sortDir->setFixedWidth(118);
    m_sortDir->setToolTip(QStringLiteral("Reverse the order"));
    sortRow->addWidget(m_sortDir);
    sortRow->addStretch(1);
    root->addWidget(m_sortBar);

    connect(m_sortBox, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (!m_layout || !m_sortBox || !m_sortDir)
            return;
        const auto sort = LayoutView::Sort(m_sortBox->itemData(index).toInt());
        m_layout->setSort(sort, m_sortDir->isChecked());
    });
    connect(m_sortDir, &QPushButton::toggled, this, [this](bool descending) {
        if (!m_layout || !m_sortBox)
            return;

        m_sortDir->setLabel(descending ? QStringLiteral("Descending")
                                       : QStringLiteral("Ascending"));
        m_sortDir->setIconId(descending ? Icons::Id::SortDesc : Icons::Id::SortAsc);
        const auto sort = LayoutView::Sort(
            m_sortBox->currentData().toInt());
        m_layout->setSort(sort, descending);
    });

    root->addSpacing(8);

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

        m_viewToggle->setLabel(list ? QStringLiteral("Map")
                                    : QStringLiteral("List"));
        m_viewToggle->setIconId(list ? Icons::Id::Grid : Icons::Id::List);
        m_layout->setMode(list ? LayoutView::Mode::List
                               : LayoutView::Mode::Map);

        if (m_sortBar)
            m_sortBar->setVisible(list);
    });
    if (m_sortBar)
        m_sortBar->setVisible(false);
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportSession);
    connect(search, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_layout->setSearch(text);
    });

    connect(m_layout, &LayoutView::entrySelected, this, [this](const QString &path) {

        if (path.isEmpty()) {
            m_inspector->setEntry(nullptr);
            return;
        }
        for (const Entry &entry : m_current.entries) {
            if (entry.relativePath == path) {
                m_inspector->setEntry(&entry);
                return;
            }
        }
        m_inspector->setEntry(nullptr);
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
    connect(m_inspector, &Inspector::openFolderRequested, this, [](const QString &folder) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
    });

    return page;
}

void MainWindow::syncSearchSuggestions()
{

    if (!m_suggestModel || !m_list)
        return;

    m_suggestModel->clear();

    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);

        if (!item || item->isHidden())
            continue;

        auto *row = new QStandardItem(item->data(Qt::UserRole + 1).toString());
        row->setData(item->data(Qt::UserRole), Qt::UserRole);
        row->setToolTip(item->data(Qt::UserRole + 2).toString());
        row->setEditable(false);
        m_suggestModel->appendRow(row);
    }

    if (m_suggestModel->rowCount() == 0) {
        const QString reason = m_list->count() == 0
            ? QStringLiteral("No snapshots yet")
            : QStringLiteral("No snapshots match");
        auto *empty = new QStandardItem(reason);
        empty->setEnabled(false);
        empty->setEditable(false);
        m_suggestModel->appendRow(empty);
    }

    int widest = m_search ? m_search->width() : 190;
    const QFontMetrics fm(m_suggest->font());
    for (int i = 0; i < m_suggestModel->rowCount(); ++i) {
        const QString text = m_suggestModel->item(i)->text();
        widest = qMax(widest, fm.horizontalAdvance(text) + 28);
    }
    m_suggest->setFixedWidth(qMin(widest, 420));
}

void MainWindow::showSearchSuggestions()
{
    if (!m_suggest || !m_search || m_suggestModel->rowCount() == 0)
        return;

    const int rows = qMin(m_suggestModel->rowCount(), 7);
    const int rowHeight = qMax(24, m_suggest->fontMetrics().height() + 12);
    m_suggest->setFixedHeight(rows * rowHeight + 8);

    auto *page = m_suggest->parentWidget();
    QPoint below = page->mapFromGlobal(
        m_search->mapToGlobal(QPoint(0, m_search->height() + 2)));

    const int margin = 8;
    below.setX(qBound(margin, below.x(),
                      qMax(margin, page->width() - m_suggest->width() - margin)));
    m_suggest->move(below);
    m_suggest->show();
    m_suggest->raise();
}

void MainWindow::hideSearchSuggestions()
{
    if (m_suggest)
        m_suggest->hide();
}

void MainWindow::activateSuggestion(int row)
{
    if (!m_suggestModel || row < 0 || row >= m_suggestModel->rowCount())
        return;

    const QString id = m_suggestModel->item(row)->data(Qt::UserRole).toString();
    hideSearchSuggestions();

    if (id.isEmpty())
        return;

    m_search->clear();
    openSessionById(id);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_search && m_search) {
        if (event->type() == QEvent::FocusIn) {
            syncSearchSuggestions();
            showSearchSuggestions();
        } else if (event->type() == QEvent::FocusOut) {

            if (m_suggest && !m_suggest->hasFocus())
                hideSearchSuggestions();
        } else if (event->type() == QEvent::KeyPress) {
            auto *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Escape) {
                hideSearchSuggestions();
                return true;
            }

            if (key->key() == Qt::Key_Down && m_suggest && m_suggest->isVisible()
                && m_suggestModel->rowCount() > 0) {
                m_suggest->setFocus();
                m_suggest->setCurrentIndex(m_suggestModel->index(0, 0));
                return true;
            }
            if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)
                && m_suggest && m_suggest->isVisible()
                && m_suggestModel->rowCount() > 0) {

                activateSuggestion(0);
                return true;
            }
        }
    }

    return QWidget::eventFilter(watched, event);
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

    syncSearchSuggestions();

    auto *count = static_cast<ElidedLabel *>(m_libraryCount);
    count->setFullText(m_sessions.isEmpty()
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

    rebuildKindFilter();
    m_status->setState(session.livePositions ? QStringLiteral("live layout")
                                             : QStringLiteral("grid layout"),
                       session.livePositions);
    m_sidebar->setActive(m_openNav);

    if (m_watcher)
        m_watcher->watch(session.rootPath);
}

void MainWindow::onFolderChanged()
{

    if (m_current.id.isEmpty())
        return;

    if (!QFileInfo(m_current.rootPath).isDir()) {
        updateLiveState();
        return;
    }

    if (m_applyingLive)
        return;
    m_applyingLive = true;

    Session updated = SessionStore::capture(m_current.rootPath);

    updated.id = m_current.id;
    updated.name = m_current.name;
    updated.capturedAt = m_current.capturedAt;

    m_current = updated;

    m_detailMeta->setText(
        QStringLiteral("%1  ·  %2  ·  %3")
            .arg(updated.id,
                 updated.summary(),
                 updated.capturedAt.toString(QStringLiteral("MMM d, yyyy  HH:mm"))));

    const QString keep = m_layout->selectedPath();

    m_layout->setSession(updated, false);
    m_inspector->setSession(updated);

    if (!keep.isEmpty()) {
        for (const Entry &entry : m_current.entries) {
            if (entry.relativePath == keep) {
                m_inspector->setEntry(&entry);
                break;
            }
        }
    }

    rebuildKindFilter();
    updateLiveState();

    m_applyingLive = false;
}

void MainWindow::updateLiveState()
{
    if (m_current.id.isEmpty())
        return;

    if (!QFileInfo(m_current.rootPath).isDir()) {
        m_status->setState(QStringLiteral("folder is gone"), true);
        m_status->setSession(QStringLiteral("%1  ·  %2").arg(m_current.name, m_current.id));
        return;
    }

    m_status->setState(m_current.livePositions ? QStringLiteral("live layout")
                                               : QStringLiteral("grid layout"),
                       m_current.livePositions);
    m_status->setSession(QStringLiteral("%1  ·  %2").arg(m_current.name, m_current.id));
}

void MainWindow::rebuildKindFilter()
{
    if (!m_kindRow)
        return;

    while (QLayoutItem *item = m_kindRow->takeAt(0)) {
        if (QWidget *w = item->widget()) {
            w->hide();
            w->setParent(nullptr);
            delete w;
        }
        delete item;
    }

    m_kindEnabled.clear();

    static const Kind kOrder[] = {Kind::Folder,   Kind::Archive, Kind::Executable,
                                  Kind::Document, Kind::Image,   Kind::Code,
                                  Kind::Audio,    Kind::Video,   Kind::Shortcut,
                                  Kind::Other};
    QList<Kind> present;
    for (Kind kind : kOrder) {
        if (m_current.countOf(kind) > 0)
            present.append(kind);
    }

    if (present.size() < 2) {
        m_kindBar->setVisible(false);
        m_layout->setKindFilter(QHash<int, bool>());
        return;
    }
    m_kindBar->setVisible(true);

    auto addChip = [this](Kind kind) {

        auto *chip = new FilterChip(kindName(kind), m_current.countOf(kind), m_kindBar);
        m_kindRow->addWidget(chip);

        connect(chip, &FilterChip::toggled, this, [this, kind](bool on) {
            m_kindEnabled.insert(int(kind), on);
            applyKindFilter();
        });
    };

    for (Kind kind : present)
        addChip(kind);

    m_kindRow->addStretch(1);

    applyKindFilter();
}

void MainWindow::applyKindFilter()
{

    bool allOn = true;
    for (auto it = m_kindEnabled.constBegin(); it != m_kindEnabled.constEnd(); ++it) {
        if (!it.value()) {
            allOn = false;
            break;
        }
    }
    m_layout->setKindFilter(allOn ? QHash<int, bool>() : m_kindEnabled);

    const int shown = m_layout->visiblePaths().size();
    m_status->setCounts(QStringLiteral("%1 of %2 items  ·  %3")
                            .arg(shown)
                            .arg(m_current.entries.size())
                            .arg(Theme::formatBytes(m_current.totalSize())));
}

void MainWindow::openSessionById(const QString &id)
{

    const bool alreadyShowing = !id.isEmpty() && id == m_current.id
        && m_pages->currentIndex() == DetailPage;
    if (alreadyShowing)
        return;

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

    if (dialog.openInNewWindow())
        emit sessionOpened(id);
    return id;
}

void MainWindow::newSnapshot()
{
    runSnapshotDialog(false);
}

bool MainWindow::geometryIsOnScreen() const
{
    const QRect frame = frameGeometry();
    for (const QScreen *screen : QGuiApplication::screens()) {

        const QRect hit = screen->availableGeometry().intersected(frame);
        if (hit.width() > 120 && hit.height() > 80)
            return true;
    }
    return false;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings().setValue(QStringLiteral("window/libraryGeometry"), saveGeometry());
    ChromeWindow::closeEvent(event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{

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

    QFile source(SessionStore::pathForId(m_current.id));
    if (!source.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("Export failed"),
                             QStringLiteral("The session file could not be read."));
        return;
    }
    const QByteArray bytes = source.readAll();

    QSaveFile out(target);
    if (!out.open(QIODevice::WriteOnly)
        || out.write(bytes) != bytes.size()
        || !out.commit()) {
        QMessageBox::warning(this, QStringLiteral("Export failed"),
                             QStringLiteral("The session file could not be written there."));
        return;
    }
    m_status->setState(QStringLiteral("exported"), true);
}

void MainWindow::setPage(Page page)
{

    hideSearchSuggestions();
    m_pages->setCurrentIndex(int(page));
    m_sidebar->setActive(page == LibraryPage ? m_libraryNav : m_openNav);
    if (page == LibraryPage)
        m_status->setState(QStringLiteral("ready"));
}
