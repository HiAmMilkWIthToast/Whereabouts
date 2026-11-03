#include "icontextbutton.h"
#include "theme.h"

#include <QApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QStyleOptionButton>

namespace {

QString declaredColor(const QString &sheet, const QString &selector)
{
    const QStringList lines = sheet.split(QLatin1Char('\n'));
    QString current;
    bool inBlock = false;
    bool matched = false;
    QString color;

    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty())
            continue;

        if (!inBlock) {
            if (!line.endsWith(QLatin1Char('{')))
                continue;

            current = line.left(line.size() - 1).trimmed();
            inBlock = true;

            matched = current.endsWith(selector);
            continue;
        }

        if (line.startsWith(QLatin1Char('}'))) {
            inBlock = false;
            continue;
        }

        if (!matched || !line.startsWith(QLatin1String("color:")))
            continue;

        QString value = line.mid(6).trimmed();
        if (value.endsWith(QLatin1Char(';')))
            value.chop(1);
        color = value.trimmed();
    }
    return color;
}

QString expandTokens(QString value)
{
    value.replace(QLatin1String("%ACCENT_CONTRAST%"), Theme::accentContrastName());
    value.replace(QLatin1String("%ACCENT_HOVER%"), Theme::accentHoverName());
    value.replace(QLatin1String("%ACCENT_DIM%"), Theme::accentDimName());
    value.replace(QLatin1String("%ACCENT%"), Theme::accentName());
    return value;
}

const QString &cachedStyleSheet()
{
    static const QApplication *owner = nullptr;
    static QString sheet;

    const QApplication *current = qApp;
    if (owner != current || sheet.isEmpty()) {
        sheet = Theme::buildStyleSheet();
        owner = current;
    }
    return sheet;
}

}

IconTextButton::IconTextButton(Icons::Id icon, const QString &text, QWidget *parent)
    : QPushButton(text, parent), m_icon(icon), m_label(text)
{

    setText(QString());
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
}

void IconTextButton::setIconId(Icons::Id id)
{
    if (m_icon == id)
        return;
    m_icon = id;
    update();
}

void IconTextButton::setLabel(const QString &text)
{
    if (m_label == text)
        return;
    m_label = text;
    updateGeometry();
    update();
}

QColor IconTextButton::inkColor() const
{
    return textColor();
}

QColor IconTextButton::textColor() const
{
    QString name = objectName();
    if (name.isEmpty())
        name = QStringLiteral("ghost");

    const QString base = QStringLiteral("#") + name;

    const QString &sheet = cachedStyleSheet();

    QString value;
    if (isDown() || isChecked())
        value = declaredColor(sheet, base + QStringLiteral(":pressed"));
    if (value.isEmpty() && m_hovered)
        value = declaredColor(sheet, base + QStringLiteral(":hover"));
    if (value.isEmpty() && !isEnabled())
        value = declaredColor(sheet, base + QStringLiteral(":disabled"));
    if (value.isEmpty())
        value = declaredColor(sheet, base);
    if (value.isEmpty())
        value = QStringLiteral("#c2c8d1");

    return QColor(expandTokens(value));
}

QSize IconTextButton::sizeHint() const
{
    const QFontMetrics fm(font());
    const int w = kGlyph + kGap + fm.horizontalAdvance(m_label);

    return QSize(w + 16 * 2 + 2, qMax(kGlyph + 12, fm.height() + 11));
}

QSize IconTextButton::minimumSizeHint() const
{
    return sizeHint();
}

void IconTextButton::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QPushButton::enterEvent(event);
}

void IconTextButton::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QPushButton::leaveEvent(event);
}

void IconTextButton::paintEvent(QPaintEvent *)
{

    QStyleOptionButton opt;
    initStyleOption(&opt);
    opt.text.clear();
    QPainter p(this);
    style()->drawControl(QStyle::CE_PushButtonBevel, &opt, &p, this);

    if ((opt.state & QStyle::State_Sunken) && !isChecked())
        return;

    const QColor ink = textColor();
    const int contentW = kGlyph + kGap + fontMetrics().horizontalAdvance(m_label);

    const int x = (width() - contentW) / 2;
    const int glyphY = (height() - kGlyph) / 2;

    QColor glyphInk = ink;
    if (!isEnabled())
        glyphInk.setAlpha(150);

    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawPixmap(x, glyphY,
                 Icons::render(m_icon, kGlyph, glyphInk, devicePixelRatioF()));

    p.setPen(ink);
    p.drawText(QRect(x + kGlyph + kGap, 0, width(), height()),
               Qt::AlignLeft | Qt::AlignVCenter, m_label);
}
