#pragma once

#include "sessionmodel.h"

#include <QWidget>

class QLabel;
class QVBoxLayout;
class QFrame;
class QPushButton;
class QScrollArea;

// Right-hand details panel for the selected item. When nothing is selected it
// shows a summary of the whole snapshot instead, so the panel is never empty.
class Inspector : public QWidget
{
    Q_OBJECT
public:
    explicit Inspector(QWidget *parent = nullptr);

    void setSession(const Session &session);
    void setEntry(const Entry *entry);

    // True when the panel describes a single entry rather than the
    // session summary. Used by tests to assert a selection was cleared.
    bool hasEntry() const { return m_hasEntry; }

signals:
    void revealRequested(const QString &name);
    void copyPathRequested(const QString &text);
    void openFolderRequested(const QString &root);

private:
    void clearBody();
    void buildSummary(const Session &session);
    void buildEntry(const Session &session, const Entry &entry);

    QLabel *addRow(QVBoxLayout *layout, const QString &key, const QString &value,
                   bool mono = false);

    QVBoxLayout *m_body = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_subtitle = nullptr;
    QWidget *m_bodyHost = nullptr;

    // The panel owns a copy of the session it is showing. Callers hand over a
    // pointer to an entry that may live in a temporary or be invalidated by a
    // later rebuild, so holding a copy is what keeps the pointers below valid.
    Session m_session;
    Entry m_entry;
    bool m_hasEntry = false;
};
