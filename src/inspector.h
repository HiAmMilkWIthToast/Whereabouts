#pragma once

#include "sessionmodel.h"

#include <QWidget>

class QLabel;
class QVBoxLayout;
class QFrame;
class QPushButton;
class QScrollArea;

class Inspector : public QWidget
{
    Q_OBJECT
public:
    explicit Inspector(QWidget *parent = nullptr);

    void setSession(const Session &session);
    void setEntry(const Entry *entry);

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

    Session m_session;
    Entry m_entry;
    bool m_hasEntry = false;
};
