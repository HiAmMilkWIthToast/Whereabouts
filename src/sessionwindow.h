#pragma once

#include "chrome.h"
#include "sessionmodel.h"

class FolderWatcher;

class SessionWindow : public ChromeWindow
{
    Q_OBJECT
public:
    explicit SessionWindow(QWidget *parent = nullptr);

    bool showSession(const QString &idOrPath);
    QString sessionId() const { return m_session.id; }

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:

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

    bool m_applyingLive = false;
};
