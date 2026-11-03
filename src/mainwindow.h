#pragma once

#include "chrome.h"
#include "sessionmodel.h"

#include <QWidget>

class QStackedWidget;
class QListWidget;
class QLineEdit;
class QLabel;
class QVBoxLayout;
class QHBoxLayout;
class QPushButton;
class IconTextButton;
class QComboBox;
class QListView;
class QStandardItemModel;
class LayoutView;
class Inspector;
class FolderWatcher;

class MainWindow : public ChromeWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    QString captureFolder(const QString &path, const QString &name,
                          bool revealInLibrary = true);

    QString runSnapshotDialog(bool firstRun = false);

    void refreshLibrary();

    void presentSession(const Session &session);

public slots:
    void newSnapshot();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void sessionOpened(const QString &sessionId);

private:
    enum Page { LibraryPage = 0, DetailPage = 1 };

    QWidget *buildLibraryPage();
    QWidget *buildDetailPage();
    void applySession(const Session &session);
    void openSessionById(const QString &id);
    void setPage(Page page);
    bool geometryIsOnScreen() const;
    void showSessionMenu(const QString &id);
    void exportSession();

    void syncSearchSuggestions();
    void showSearchSuggestions();
    void hideSearchSuggestions();

    void activateSuggestion(int row);

    void rebuildKindFilter();
    void applyKindFilter();

    void onFolderChanged();
    void updateLiveState();

    QStackedWidget *m_pages = nullptr;
    Sidebar *m_sidebar = nullptr;
    StatusBar *m_status = nullptr;

    QListWidget *m_list = nullptr;
    QLineEdit *m_search = nullptr;

    QLabel *m_libraryCount = nullptr;
    QLabel *m_welcome = nullptr;
    QListView *m_suggest = nullptr;
    QStandardItemModel *m_suggestModel = nullptr;
    QList<Session> m_sessions;

    QLabel *m_detailTitle = nullptr;
    QLabel *m_detailMeta = nullptr;
    QLineEdit *m_find = nullptr;
    LayoutView *m_layout = nullptr;
    Inspector *m_inspector = nullptr;
    IconTextButton *m_viewToggle = nullptr;
    QWidget *m_kindBar = nullptr;
    QHBoxLayout *m_kindRow = nullptr;
    QWidget *m_sortBar = nullptr;
    QComboBox *m_sortBox = nullptr;
    IconTextButton *m_sortDir = nullptr;

    QHash<int, bool> m_kindEnabled;

    FolderWatcher *m_watcher = nullptr;

    bool m_applyingLive = false;

    Session m_current;
    int m_libraryNav = 0;
    int m_newNav = 0;
    int m_openNav = 0;
};
