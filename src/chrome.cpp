#include "chrome.h"
#include "theme.h"

#include <QCloseEvent>
#include <QDate>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QShowEvent>
#include <QStyle>
#include <QTime>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QWindow>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>
#endif

namespace {

constexpr int kResizeBorder = 8;

constexpr char kProjectUrl[] = "https://github.com/HiAmMilkWIthToast/Whereabouts";

void applyDarkCaption(QWidget *widget)
{
#ifdef Q_OS_WIN
    if (!widget || !widget->windowHandle())
        return;
    const HWND hwnd = reinterpret_cast<HWND>(widget->windowHandle()->winId());
    const BOOL dark = TRUE;

    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark))))
        DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
#else
    Q_UNUSED(widget);
#endif
}

}

IconButton::IconButton(Icons::Id id, int logicalSize, QWidget *parent)
    : QWidget(parent), m_id(id), m_size(logicalSize)
{
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
    m_mask = Icons::alphaMask(m_id, m_size, devicePixelRatioF());

    m_hoverAnim = new QVariantAnimation(this);
    m_hoverAnim->setDuration(140);
    m_hoverAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_hoverAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_hover = v.toReal();
        update();
    });

    m_activeAnim = new QVariantAnimation(this);
    m_activeAnim->setDuration(160);
    m_activeAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_activeAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_active = v.toReal();
        update();
    });
}

QSize IconButton::sizeHint() const
{
    return QSize(m_size + 18, m_size + 18);
}

void IconButton::setChecked(bool checked)
{
    if (m_checked == checked)
        return;
    m_checked = checked;
    animate(m_active, checked ? 1.0 : 0.0, m_activeAnim, 160);
}

void IconButton::setIcon(Icons::Id id)
{
    if (m_id == id)
        return;
    m_id = id;
    m_mask = Icons::alphaMask(m_id, m_size, devicePixelRatioF());
    update();
}

void IconButton::setTooltip(const QString &text)
{
    setToolTip(text);
}

void IconButton::animate(qreal &value, qreal target, QVariantAnimation *&animation,
                         int duration)
{
    animation->stop();
    animation->setStartValue(value);
    animation->setEndValue(target);
    animation->setDuration(duration);
    animation->start();
}

void IconButton::enterEvent(QEnterEvent *event)
{
    animate(m_hover, 1.0, m_hoverAnim, 140);
    QWidget::enterEvent(event);
}

void IconButton::leaveEvent(QEvent *event)
{
    animate(m_hover, 0.0, m_hoverAnim, 140);
    QWidget::leaveEvent(event);
}

void IconButton::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = true;
        update();

        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void IconButton::mouseReleaseEvent(QMouseEvent *event)
{
    const bool wasPressed = m_pressed;
    m_pressed = false;
    update();
    if (event->button() == Qt::LeftButton) {
        if (wasPressed && rect().contains(event->pos()))
            emit clicked();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void IconButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = QRectF(rect()).adjusted(2, 2, -2, -2);

    QColor base = Theme::accent();
    QColor bg;
    if (m_checked)
        bg = QColor(base.red(), base.green(), base.blue(), int(26 + 22 * m_hover));
    else
        bg = QColor(255, 255, 255, int(9 * m_hover));

    if (m_pressed)
        bg = Theme::lerpColor(bg, QColor(0, 0, 0, 40), 0.6);

    if (bg.alpha() > 0) {
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawRoundedRect(r, 6.0, 6.0);
    }

    QColor glyph = Theme::lerpColor(QColor(QStringLiteral("#8b929c")),
                                    QColor(QStringLiteral("#e6e9ee")), m_hover);
    if (m_checked)
        glyph = Theme::lerpColor(Theme::accent(), QColor(QStringLiteral("#ffffff")),
                                 m_hover * 0.4);
    if (!isEnabled())
        glyph = QColor(QStringLiteral("#4c525a"));

    if (!m_mask.isNull()) {
        const QSizeF s = m_mask.deviceIndependentSize();
        const QRectF target(r.center().x() - s.width() / 2.0,
                            r.center().y() - s.height() / 2.0,
                            s.width(), s.height());
        p.drawPixmap(target, Icons::tint(m_mask, glyph), QRectF(m_mask.rect()));
    }

    if (m_checked && m_active > 0.0) {
        QColor bar = Theme::accent();
        bar.setAlphaF(m_active);
        p.setPen(Qt::NoPen);
        p.setBrush(bar);
        p.drawRoundedRect(QRectF(2.0, r.top() + 7.0, 2.0, r.height() - 14.0), 1.0, 1.0);
    }
}

class TitleBar::BrandButton : public QWidget
{
public:
    BrandButton(QWidget *parent)
        : QWidget(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
        setFocusPolicy(Qt::NoFocus);
        m_mask = Icons::alphaMask(Icons::Id::Cairn, kMarkSize, devicePixelRatioF());

        m_hoverAnim = new QVariantAnimation(this);
        m_hoverAnim->setDuration(150);
        m_hoverAnim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_hoverAnim, &QVariantAnimation::valueChanged, this,
                [this](const QVariant &v) {
                    m_hover = v.toReal();
                    update();
                });

        m_pressAnim = new QVariantAnimation(this);
        m_pressAnim->setDuration(90);
        m_pressAnim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_pressAnim, &QVariantAnimation::valueChanged, this,
                [this](const QVariant &v) {
                    m_press = v.toReal();
                    update();
                });
    }

    void setLink(const QUrl &url)
    {
        m_url = url;
        setCursor(url.isValid() && !url.isEmpty() ? Qt::PointingHandCursor
                                                  : Qt::ArrowCursor);
        update();
    }

    QSize sizeHint() const override
    {
        const QFontMetrics fm(font());
        return QSize(kPadX * 2 + kMarkSize + 7 + fm.horizontalAdvance(kName),
                     Metrics::kTitleBarHeight);
    }

protected:
    void enterEvent(QEnterEvent *event) override
    {
        animate(m_hover, 1.0, m_hoverAnim, 150);
        QWidget::enterEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        animate(m_hover, 0.0, m_hoverAnim, 150);
        QWidget::leaveEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
            animate(m_press, 1.0, m_pressAnim, 90);
        QWidget::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        animate(m_press, 0.0, m_pressAnim, 90);
        if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())
            && !m_url.isEmpty()) {
            QDesktopServices::openUrl(m_url);
        }
        QWidget::mouseReleaseEvent(event);
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const qreal lift = m_hover * (1.0 - m_press * 0.5);
        const QRectF pill = QRectF(rect()).adjusted(2, 4, -2, -4);
        const int alpha = int(26 * lift);
        if (alpha > 0) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(208, 168, 106, alpha));
            p.drawRoundedRect(pill, 6, 6);
        }

        const int markY = (height() - kMarkSize) / 2 - int(m_hover * 1.0);
        const QColor markColor = Theme::lerpColor(
            Theme::brand(), Theme::brand().lighter(135), qMax(m_hover, m_press));
        p.drawPixmap(kPadX, markY, Icons::tint(m_mask, markColor));

        const int textX = kPadX + kMarkSize + 7;
        QFont f = font();
        f.setPixelSize(12);
        f.setWeight(QFont::DemiBold);
        p.setFont(f);
        const QColor nameColor = Theme::lerpColor(
            QColor(QStringLiteral("#eceef2")), markColor, qMax(m_hover, m_press));
        p.setPen(nameColor);
        p.drawText(QRect(textX, 0, width() - textX - kPadX, height()),
                   Qt::AlignLeft | Qt::AlignVCenter, kName);
    }

private:
    static constexpr int kMarkSize = 17;
    static constexpr int kPadX = 8;
    static inline const QString kName = QStringLiteral("Cairn");

    void animate(qreal &value, qreal target, QVariantAnimation *animation, int duration)
    {
        animation->stop();
        animation->setStartValue(value);
        animation->setEndValue(target);
        animation->setDuration(duration);
        animation->start();
    }

    QUrl m_url;
    QPixmap m_mask;
    qreal m_hover = 0.0;
    qreal m_press = 0.0;
    QVariantAnimation *m_hoverAnim = nullptr;
    QVariantAnimation *m_pressAnim = nullptr;
};

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("titleBar"));
    setFixedHeight(Metrics::kTitleBarHeight);
    setAttribute(Qt::WA_StyledBackground, true);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 0, 8, 0);
    layout->setSpacing(8);

    m_brand = new BrandButton(this);
    m_brand->setObjectName(QStringLiteral("appBrand"));
    m_brand->setToolTip(QStringLiteral("Open the project page"));
    layout->addWidget(m_brand);

    m_subtitle = new QLabel(this);
    m_subtitle->setObjectName(QStringLiteral("appTag"));
    layout->addWidget(m_subtitle);

    layout->addStretch(1);

    m_center = new QHBoxLayout;
    m_center->setContentsMargins(0, 0, 0, 0);
    m_center->setSpacing(6);
    layout->addLayout(m_center);

    layout->addStretch(1);

    auto *minimize = new IconButton(Icons::Id::Minimize, 14, this);
    minimize->setTooltip(QStringLiteral("Minimize"));
    minimize->setFixedSize(28, 24);
    m_maximize = new IconButton(Icons::Id::Maximize, 13, this);
    m_maximize->setTooltip(QStringLiteral("Maximize"));
    m_maximize->setFixedSize(28, 24);
    auto *close = new IconButton(Icons::Id::Close, 14, this);
    close->setTooltip(QStringLiteral("Close"));
    close->setFixedSize(28, 24);

    layout->addWidget(minimize);
    layout->addWidget(m_maximize);
    layout->addWidget(close);

    connect(minimize, &IconButton::clicked, this, &TitleBar::minimizeRequested);
    connect(m_maximize, &IconButton::clicked, this, &TitleBar::maximizeRequested);
    connect(close, &IconButton::clicked, this, &TitleBar::closeRequested);
}

void TitleBar::setBrandLink(const QUrl &url)
{
    if (m_brand)
        m_brand->setLink(url);
}

void TitleBar::setMaximized(bool maximized)
{
    if (!m_maximize)
        return;
    m_maximize->setIcon(maximized ? Icons::Id::Restore : Icons::Id::Maximize);
    m_maximize->setTooltip(maximized ? QStringLiteral("Restore")
                                     : QStringLiteral("Maximize"));
}

void TitleBar::setSubtitle(const QString &text)
{
    m_subtitle->setText(text);
    m_subtitle->setVisible(!text.isEmpty());
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{

    if (event->button() == Qt::LeftButton && !childAt(event->pos())
        && window() && window()->windowHandle()) {
        window()->windowHandle()->startSystemMove();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QWidget *w = window();
        if (w) {
            if (w->isMaximized())
                w->showNormal();
            else
                w->showMaximized();
            event->accept();
            return;
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

Sidebar::Sidebar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("sidebar"));
    setFixedWidth(Metrics::kSidebarWidth);
    setAttribute(Qt::WA_StyledBackground, true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 8, 4, 8);
    layout->setSpacing(4);
    layout->addStretch(1);
}

int Sidebar::addPage(Icons::Id icon, const QString &tooltip)
{
    auto *box = qobject_cast<QVBoxLayout *>(layout());
    auto *button = new IconButton(icon, 17, this);
    button->setTooltip(tooltip);
    button->setFixedSize(38, 34);

    box->insertWidget(box->count() - 1, button);

    const int id = m_buttons.size();
    m_buttons.append(button);
    connect(button, &IconButton::clicked, this, [this, id] { emit pageRequested(id); });
    return id;
}

void Sidebar::setActive(int id)
{
    for (int i = 0; i < m_buttons.size(); ++i)
        m_buttons.at(i)->setChecked(i == id);
}

StatusBar::StatusBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("statusBar"));
    setFixedHeight(Metrics::kStatusBarHeight);
    setAttribute(Qt::WA_StyledBackground, true);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 0, 12, 0);
    layout->setSpacing(14);

    m_state = new QLabel(this);
    m_state->setObjectName(QStringLiteral("statusAccent"));

    m_state->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    layout->addWidget(m_state);

    m_session = new QLabel(this);
    m_session->setObjectName(QStringLiteral("statusText"));
    m_session->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    layout->addWidget(m_session);

    layout->addStretch(1);

    m_counts = new QLabel(this);
    m_counts->setObjectName(QStringLiteral("statusValue"));
    m_counts->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    layout->addWidget(m_counts);

    m_hint = new QLabel(this);
    m_hint->setObjectName(QStringLiteral("statusText"));
    m_hint->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    layout->addWidget(m_hint);

    m_date = new QLabel(this);
    m_date->setObjectName(QStringLiteral("statusText"));
    m_date->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    layout->addWidget(m_date);

    m_clock = new QLabel(this);
    m_clock->setObjectName(QStringLiteral("statusValue"));
    m_clock->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    layout->addWidget(m_clock);

    m_tick = new QTimer(this);
    m_tick->setInterval(1000);
    connect(m_tick, &QTimer::timeout, this, [this] {
        refreshClock();
        refreshDate();
    });
    refreshClock();
    refreshDate();
    setClockVisible(true);
}

void StatusBar::refreshClock()
{
    m_clock->setText(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")));
}

void StatusBar::refreshDate()
{

    m_date->setText(QDate::currentDate().toString(QStringLiteral("ddd d MMM yyyy")));
}

void StatusBar::setClockVisible(bool visible)
{
    m_date->setVisible(visible);
    m_clock->setVisible(visible);

    if (visible && !m_tick->isActive())
        m_tick->start();
    else if (!visible)
        m_tick->stop();
}

void StatusBar::setState(const QString &text, bool highlight)
{
    m_state->setText(text);
    const QString wanted = highlight ? QStringLiteral("statusAccent")
                                     : QStringLiteral("statusText");
    if (m_state->objectName() != wanted) {
        m_state->setObjectName(wanted);
        m_state->style()->unpolish(m_state);
        m_state->style()->polish(m_state);
    }
}

void StatusBar::setSession(const QString &text) { m_session->setText(text); }
void StatusBar::setCounts(const QString &text) { m_counts->setText(text); }
void StatusBar::setHint(const QString &text) { m_hint->setText(text); }

ChromeWindow::ChromeWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlag(Qt::FramelessWindowHint, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMinimumSize(560, 420);

    m_frameLayout = new QVBoxLayout(this);
    m_frameLayout->setContentsMargins(6, 6, 6, 6);
    m_frameLayout->setSpacing(0);
}

TitleBar *ChromeWindow::createTitleBar()
{

    if (m_titleBar)
        return m_titleBar;

    m_titleBar = new TitleBar(this);
    m_titleBar->setBrandLink(QUrl(QLatin1String(kProjectUrl)));
    m_frameLayout->addWidget(m_titleBar);

    connect(m_titleBar, &TitleBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(m_titleBar, &TitleBar::closeRequested, this, &QWidget::close);
    connect(m_titleBar, &TitleBar::maximizeRequested, this,
            &ChromeWindow::toggleMaximized);
    return m_titleBar;
}

void ChromeWindow::toggleMaximized()
{
    if (isMaximized())
        showNormal();
    else
        showMaximized();
}

void ChromeWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(QStringLiteral("#101216")));

    const qreal radius = isMaximized() ? 0.0 : 9.0;
    p.drawRoundedRect(QRectF(rect()).adjusted(0, 0, -1, -1), radius, radius);

    if (!isMaximized()) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(QStringLiteral("#252a31")), 1.0));
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    }
}

void ChromeWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    applyDarkCaption(this);

#ifdef Q_OS_WIN
    if (m_frameApplied)
        return;
    m_frameApplied = true;

    HWND hwnd = reinterpret_cast<HWND>(winId());
    const LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    SetWindowLongPtr(hwnd, GWL_STYLE, style | WS_THICKFRAME);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
                     | SWP_FRAMECHANGED);
#endif
}

void ChromeWindow::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {

        const int inset = isMaximized() ? 0 : 6;
        m_frameLayout->setContentsMargins(inset, inset, inset, inset);
        if (m_titleBar)
            m_titleBar->setMaximized(isMaximized());
        update();
    }
}

void ChromeWindow::closeEvent(QCloseEvent *event)
{
    QWidget::closeEvent(event);
}

#ifdef Q_OS_WIN
bool ChromeWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(eventType);
    MSG *msg = static_cast<MSG *>(message);

    if (msg->message == WM_NCCALCSIZE && msg->wParam != FALSE) {
        *result = 0;
        return true;
    }

    if (msg->message == WM_NCHITTEST && !isMaximized()) {

        const int x = GET_X_LPARAM(msg->lParam);
        const int y = GET_Y_LPARAM(msg->lParam);

        RECT r;
        if (GetWindowRect(reinterpret_cast<HWND>(winId()), &r)) {
            const bool left   = x >= r.left && x < r.left + kResizeBorder;
            const bool right  = x <= r.right && x > r.right - kResizeBorder;
            const bool top    = y >= r.top && y < r.top + kResizeBorder;
            const bool bottom = y <= r.bottom && y > r.bottom - kResizeBorder;

            if (left && top)     { *result = HTTOPLEFT;     return true; }
            if (right && top)    { *result = HTTOPRIGHT;    return true; }
            if (left && bottom)  { *result = HTBOTTOMLEFT;  return true; }
            if (right && bottom) { *result = HTBOTTOMRIGHT; return true; }
            if (left)            { *result = HTLEFT;        return true; }
            if (right)           { *result = HTRIGHT;       return true; }
            if (top)             { *result = HTTOP;         return true; }
            if (bottom)          { *result = HTBOTTOM;      return true; }
        }
    }
    return false;
}
#endif
