#pragma once

#include "icons.h"

#include <QColor>
#include <QWidget>

class QLabel;
class QTimer;
class QVBoxLayout;
class QHBoxLayout;
class QVariantAnimation;

// Icon-only button that animates hover, armed and checked states, and tints its
// glyph with the accent colour when active. Used by the sidebar and title bar.
class IconButton : public QWidget
{
    Q_OBJECT
public:
    explicit IconButton(Icons::Id id, int logicalSize = 16, QWidget *parent = nullptr);

    void setChecked(bool checked);
    bool isChecked() const { return m_checked; }

    void setTooltip(const QString &text);

    // Chosen glyph, so a window control can show Maximize or Restore.
    void setIcon(Icons::Id id);
    Icons::Id icon() const { return m_id; }

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    QSize sizeHint() const override;

private:
    void animate(qreal &value, qreal target, QVariantAnimation *&animation,
                 int duration);

    Icons::Id m_id;
    int m_size = 16;
    bool m_checked = false;
    bool m_pressed = false;
    QPixmap m_mask;
    qreal m_hover = 0.0;
    qreal m_active = 0.0;
    QVariantAnimation *m_hoverAnim = nullptr;
    QVariantAnimation *m_activeAnim = nullptr;
};

// Frameless, draggable custom title bar: the mark, the app name, and the
// standard window controls.
//
// The controls only emit signals; ChromeWindow binds them to the real window
// actions, so adding a title bar is enough to get working buttons.
class TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(QWidget *parent = nullptr);

    // Optional middle slot, filled by the window (search box, session label).
    QHBoxLayout *centerLayout() const { return m_center; }

    void setSubtitle(const QString &text);

    // Keeps the maximize glyph honest when the window changes state.
    void setMaximized(bool maximized);

signals:
    void minimizeRequested();
    void maximizeRequested();
    void closeRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    QLabel *m_subtitle = nullptr;
    QHBoxLayout *m_center = nullptr;
    IconButton *m_maximize = nullptr;
};

// The narrow icon rail. A button is added per page and shown as active when its
// page is current.
class Sidebar : public QWidget
{
    Q_OBJECT
public:
    explicit Sidebar(QWidget *parent = nullptr);

    // Returns the id to pass to setActive().
    int addPage(Icons::Id icon, const QString &tooltip);
    void setActive(int id);

signals:
    void pageRequested(int id);

private:
    QVector<IconButton *> m_buttons;
};

// Thin footer with a handful of live facts about what is loaded.
class StatusBar : public QWidget
{
    Q_OBJECT
public:
    explicit StatusBar(QWidget *parent = nullptr);

    void setState(const QString &text, bool highlight = false);
    void setSession(const QString &text);
    void setCounts(const QString &text);
    void setHint(const QString &text);

    // The clock shows local time and updates itself; the caller only decides
    // whether to show it.
    void setClockVisible(bool visible);

private:
    void refreshClock();
    void refreshDate();

    QLabel *m_state = nullptr;
    QLabel *m_session = nullptr;
    QLabel *m_counts = nullptr;
    QLabel *m_hint = nullptr;
    QLabel *m_date = nullptr;
    QLabel *m_clock = nullptr;
    QTimer *m_tick = nullptr;
};

// Base class for every top-level window in the app: rounded frameless shell
// with native edge resizing, a dark caption, and drag/zoom via the title bar.
//
// Subclasses only add their title bar and body; the window controls, dragging,
// double-click-to-maximize, resizing and state tracking all live here so no
// window can silently end up with dead buttons.
class ChromeWindow : public QWidget
{
    Q_OBJECT
public:
    explicit ChromeWindow(QWidget *parent = nullptr);

    // The vertical layout inside the rounded frame; callers add their body here.
    QVBoxLayout *frameLayout() const { return m_frameLayout; }

    // The title bar to embed. ChromeWindow connects its controls.
    TitleBar *createTitleBar();

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

private:
    void toggleMaximized();

    QVBoxLayout *m_frameLayout = nullptr;
    TitleBar *m_titleBar = nullptr;
#ifdef Q_OS_WIN
    bool m_frameApplied = false;
#endif
};
