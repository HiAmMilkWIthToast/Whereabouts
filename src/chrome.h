#pragma once

#include "icons.h"

#include <QColor>
#include <QUrl>
#include <QWidget>

class QLabel;
class QTimer;
class QVBoxLayout;
class QHBoxLayout;
class QVariantAnimation;

class IconButton : public QWidget
{
    Q_OBJECT
public:
    explicit IconButton(Icons::Id id, int logicalSize = 16, QWidget *parent = nullptr);

    void setChecked(bool checked);
    bool isChecked() const { return m_checked; }

    void setTooltip(const QString &text);

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

class TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(QWidget *parent = nullptr);

    QHBoxLayout *centerLayout() const { return m_center; }

    void setSubtitle(const QString &text);

    void setMaximized(bool maximized);

    void setBrandLink(const QUrl &url);

signals:
    void minimizeRequested();
    void maximizeRequested();
    void closeRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:

    class BrandButton;

    QLabel *m_subtitle = nullptr;
    QHBoxLayout *m_center = nullptr;
    IconButton *m_maximize = nullptr;
    BrandButton *m_brand = nullptr;
};

class Sidebar : public QWidget
{
    Q_OBJECT
public:
    explicit Sidebar(QWidget *parent = nullptr);

    int addPage(Icons::Id icon, const QString &tooltip);
    void setActive(int id);

signals:
    void pageRequested(int id);

private:
    QVector<IconButton *> m_buttons;
};

class StatusBar : public QWidget
{
    Q_OBJECT
public:
    explicit StatusBar(QWidget *parent = nullptr);

    void setState(const QString &text, bool highlight = false);
    void setSession(const QString &text);
    void setCounts(const QString &text);
    void setHint(const QString &text);

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

class ChromeWindow : public QWidget
{
    Q_OBJECT
public:
    explicit ChromeWindow(QWidget *parent = nullptr);

    QVBoxLayout *frameLayout() const { return m_frameLayout; }

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
