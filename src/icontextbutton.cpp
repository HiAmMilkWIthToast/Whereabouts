#include "icontextbutton.h"
#include "theme.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPixmap>
#include <QStyleOptionButton>

namespace {

// Finds the `color` declared for exactly this selector in the stylesheet.
//
// The alternative -- reading widget->palette() -- does not work: Qt parses QSS
// at paint time, so the palette is still the default and every glyph would come
// out black. Reading the sheet keeps the stylesheet authoritative.
//
// This walks the text line by line rather than using a regular expression. A
// regex over the whole sheet was tried first and was wrong twice: it treated
// ":disabled" as an optional suffix, so the resting state picked up the disabled
// grey, and it then failed to match at all once the type selector in front of
// the id was taken into account. A line scan has neither failure mode.
QString declaredColor(const QString &sheet, const QString &selector)
{
    const QStringList lines = sheet.split(QLatin1Char('\n'));
    QString current;          // selector of the block being read
    bool inBlock = false;
    bool matched = false;
    QString color;            // last match wins, as in the cascade

    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty())
            continue;

        if (!inBlock) {
            if (!line.endsWith(QLatin1Char('{')))
                continue;
            // "QPushButton#secondary:hover {" -> "QPushButton#secondary:hover"
            current = line.left(line.size() - 1).trimmed();
            inBlock = true;
            // The selector must END with what was asked for. A plain "contains"
            // also matched "QPushButton#secondary:hover" when the resting rule
            // was wanted, and hover rules come later in the sheet, so they won.
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

// The sheet is written with %TOKEN% placeholders; resolve the ones that can
// appear in a colour.
QString expandTokens(QString value)
{
    value.replace(QLatin1String("%ACCENT_CONTRAST%"), Theme::accentContrastName());
    value.replace(QLatin1String("%ACCENT_HOVER%"), Theme::accentHoverName());
    value.replace(QLatin1String("%ACCENT_DIM%"), Theme::accentDimName());
    value.replace(QLatin1String("%ACCENT%"), Theme::accentName());
    return value;
}

} // namespace

IconTextButton::IconTextButton(Icons::Id icon, const QString &text, QWidget *parent)
    : QPushButton(text, parent), m_icon(icon), m_label(text)
{
    // The label is painted, not handed to the style; keeping it on the widget
    // as well would make the style draw it a second time.
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
    const QString sheet = Theme::buildStyleSheet();

    // Most specific state first, falling back to the resting rule -- the order
    // the stylesheet itself resolves in.
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
    // 16px of padding each side matches the QSS rules, plus the 1px borders.
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
    // The frame and background come from the stylesheet, exactly as they would
    // for a plain QPushButton.
    QStyleOptionButton opt;
    initStyleOption(&opt);
    opt.text.clear();
    QPainter p(this);
    style()->drawControl(QStyle::CE_PushButtonBevel, &opt, &p, this);

    if ((opt.state & QStyle::State_Sunken) && !isChecked())
        return;

    const QColor ink = textColor();
    const int contentW = kGlyph + kGap + fontMetrics().horizontalAdvance(m_label);
    // Integer division keeps the pair centred in an odd-width button instead of
    // drifting half a pixel right.
    const int x = (width() - contentW) / 2;
    const int glyphY = (height() - kGlyph) / 2;

    // Disabled glyphs dim with the text, or the icon reads as enabled next to a
    // greyed-out label.
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
