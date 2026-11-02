#pragma once

#include "chrome.h"
#include "sessionmodel.h"

class FolderWatcher;

// A window that shows a session and nothing else.
//
// The point of the separate window is that it can be kept open on a second
// screen while the library does something else, and it takes a handle on the
// command line so a .session file can be opened directly.
class SessionWindow : public ChromeWindow
{
    Q_OBJECT
public:
    explicit SessionWindow(QWidget *parent = nullptr);

    // Loads the session and shows the window. Returns false (and leaves the
    // window hidden) when the file could not be read.
    bool showSession(const QString &idOrPath);
    QString sessionId() const { return m_session.id; }

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    // Re-scans the folder and refreshes the view, keeping the snapshot's
    // identity. Same job as the library window's handler of the same name.
    void onFolderChanged();
    void updateLiveState();

    Session m_session;
    class LayoutView *m_layout = nullptr;
    class Inspector *m_inspector = nullptr;
    class QLabel *m_title = nullptr;
    class QLabel *m_meta = nullptr;
    class QLineEdit *m_search = nullptr;
    class StatusBar *m_status = nullptr;

    FolderWatcher *m_watcher = nullptr;
    // Rebuilding the view touches widgets, and a change raised while that
    // happens must not start a second scan on top of the first.
    bool m_applyingLive = false;
};
