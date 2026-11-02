#pragma once

#include "chrome.h"
#include "sessionmodel.h"

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
    Session m_session;
    class LayoutView *m_layout = nullptr;
    class Inspector *m_inspector = nullptr;
    class QLabel *m_title = nullptr;
    class QLabel *m_meta = nullptr;
    class QLineEdit *m_search = nullptr;
    class StatusBar *m_status = nullptr;
};
