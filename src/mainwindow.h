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
class QComboBox;
class LayoutView;
class Inspector;
class FolderWatcher;

// The library window: every .session file the app has made, a card list of
// them, and an embedded view of the selected one.
class MainWindow : public ChromeWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    // Captures a folder and shows the result. Returns the saved session's id,
    // or an empty string when the capture failed.
    QString captureFolder(const QString &path, const QString &name,
                          bool revealInLibrary = true);

    // Runs the "snapshot a folder" dialog and captures what it returns. Returns
    // the new handle, or an empty string if the dialog was cancelled or the
    // capture failed. `firstRun` only changes the dialog's wording.
    QString runSnapshotDialog(bool firstRun = false);

    void refreshLibrary();

    // Shows a session that is already in memory, without touching the disk.
    // Used by the preview tool and by the caller that just captured.
    void presentSession(const Session &session);

public slots:
    void newSnapshot();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

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

    // Rebuilds the kind-filter chips for the current session and pushes the
    // resulting filter into the map.
    void rebuildKindFilter();
    void applyKindFilter();

    // Re-scans the folder on screen after the watcher reports a change, and
    // redraws in place. The .session file on disk is deliberately not touched:
    // live updates follow the folder, saving stays an explicit action.
    void onFolderChanged();
    void updateLiveState();

    QStackedWidget *m_pages = nullptr;
    Sidebar *m_sidebar = nullptr;
    StatusBar *m_status = nullptr;

    // library
    QListWidget *m_list = nullptr;
    QLineEdit *m_search = nullptr;
    QLabel *m_libraryCount = nullptr;
    QLabel *m_welcome = nullptr;
    QList<Session> m_sessions;

    // detail
    QLabel *m_detailTitle = nullptr;
    QLabel *m_detailMeta = nullptr;
    QLineEdit *m_find = nullptr;
    LayoutView *m_layout = nullptr;
    Inspector *m_inspector = nullptr;
    QPushButton *m_viewToggle = nullptr;
    QWidget *m_kindBar = nullptr;
    QHBoxLayout *m_kindRow = nullptr;
    QWidget *m_sortBar = nullptr;
    QComboBox *m_sortBox = nullptr;
    QPushButton *m_sortDir = nullptr;
    // Only kinds switched off are recorded; an absent kind is on.
    QHash<int, bool> m_kindEnabled;

    FolderWatcher *m_watcher = nullptr;
    // Set while a live re-scan is in flight, so a change raised by our own
    // redraw cannot feed back into another scan.
    bool m_applyingLive = false;

    Session m_current;
    int m_libraryNav = 0;   // sidebar ids, assigned in the constructor
    int m_newNav = 0;
    int m_openNav = 0;
};
