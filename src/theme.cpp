#include "theme.h"

#include <QColor>
#include <QSettings>
#include <QStringList>

namespace {

// Cached because paintEvent and the stylesheet builder both read it; QSettings
// lookups are comparatively slow.
QColor &accentCache()
{
    static QColor cached = []() {
        const QString stored =
            QSettings().value(QStringLiteral("appearance/accent")).toString();
        const QColor parsed(stored);
        return parsed.isValid() ? parsed : QColor(QStringLiteral("#6d8cf0"));
    }();
    return cached;
}

QColor blend(const QColor &a, const QColor &b, qreal t)
{
    return Theme::lerpColor(a, b, t);
}

} // namespace

namespace Theme {

QColor accent()
{
    return accentCache();
}

void setAccent(const QColor &color)
{
    if (!color.isValid())
        return;
    accentCache() = color;
    QSettings().setValue(QStringLiteral("appearance/accent"), color.name());
}

QString accentName()
{
    return accentCache().name();
}

// Darkened toward the panel colour rather than made transparent, so borders
// still look like a colour against the near-black surfaces.
QString accentDimName()
{
    return blend(accentCache(), QColor(QStringLiteral("#14161a")), 0.6).name();
}

QString accentHoverName()
{
    return blend(accentCache(), QColor(QStringLiteral("#ffffff")), 0.16).name();
}

QString accentContrastName()
{
    // Text drawn on top of an accent fill.
    return accentCache().lightness() > 170 ? QStringLiteral("#0d0f12")
                                          : QStringLiteral("#ffffff");
}

QString formatBytes(qint64 bytes)
{
    if (bytes < 0)
        return QStringLiteral("—");
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024)
        return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QStringLiteral("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

QString buildStyleSheet()
{
    // Placeholders instead of raw values so one accent change repaints the
    // whole app. Dim/hover must be substituted before the plain token, since
    // "%ACCENT_DIM%" would otherwise leave a dangling suffix.
    static const char *kTemplate = R"(
* {
    font-family: "Segoe UI", "Inter", "Microsoft YaHei", sans-serif;
}
QWidget {
    color: #c9ccd2;
    font-size: 12px;
}
QToolTip {
    background: #1c1f24;
    color: #d7dae0;
    border: 1px solid #2c3138;
    border-radius: 4px;
    padding: 4px 7px;
    font-size: 11px;
}

/* ---------- Window chrome ---------- */
QWidget#titleBar {
    background: #101216;
    border-bottom: 1px solid #1b1e23;
}
QLabel#appMark {
    color: %ACCENT%;
}
QLabel#appName {
    color: #eceef2;
    font-size: 12px;
    font-weight: 600;
}
QLabel#appTag {
    color: #5c626b;
    font-size: 10px;
}
QWidget#sidebar {
    background: #101216;
    border-right: 1px solid #1b1e23;
}
QWidget#statusBar {
    background: #101216;
    border-top: 1px solid #1b1e23;
}
QLabel#statusText {
    color: #6b727c;
    font-size: 10px;
}
QLabel#statusValue {
    color: #9aa1ab;
    font-size: 10px;
}
QLabel#statusAccent {
    color: %ACCENT%;
    font-size: 10px;
}

/* ---------- Pages ---------- */
QStackedWidget#pages {
    background: #0b0d10;
}
QLabel#pageTitle {
    color: #f2f4f8;
    font-size: 17px;
    font-weight: 600;
}
QLabel#pageSubtitle {
    color: #767d87;
    font-size: 11px;
}
QLabel#sectionTitle {
    color: #a8aeb8;
    font-size: 10px;
    font-weight: 600;
    letter-spacing: 1px;
}
QLabel#fieldLabel {
    color: #6f767f;
    font-size: 10px;
}
QLabel#fieldValue {
    color: #cdd2d9;
    font-size: 11px;
}
QLabel#mono {
    color: #9aa1ab;
    font-family: "Cascadia Mono", "Consolas", monospace;
    font-size: 11px;
}
QLabel#hint {
    color: #666d76;
    font-size: 11px;
}
QLabel#emptyHint {
    color: #5b616a;
    font-size: 11px;
}

/* ---------- Surfaces ---------- */
QFrame#card {
    background: #13161a;
    border: 1px solid #20242a;
    border-radius: 8px;
}
QFrame#cardFlat {
    background: #101317;
    border: 1px solid #1d2126;
    border-radius: 6px;
}
QFrame#divider {
    background: #1d2126;
    border: none;
}

/* ---------- Inputs ---------- */
QLineEdit#field {
    background: #0e1013;
    border: 1px solid #262b32;
    border-radius: 6px;
    color: #e4e7ec;
    font-size: 11px;
    padding: 4px 8px;
    selection-background-color: %ACCENT_DIM%;
    selection-color: #ffffff;
}
QLineEdit#field:focus {
    border-color: %ACCENT%;
}
QLineEdit#monoField {
    background: #0e1013;
    border: 1px solid #262b32;
    border-radius: 6px;
    color: #d6dbe3;
    font-family: "Cascadia Mono", "Consolas", monospace;
    font-size: 11px;
    padding: 4px 8px;
    selection-background-color: %ACCENT_DIM%;
    selection-color: #ffffff;
}
QLineEdit#monoField:focus {
    border-color: %ACCENT%;
}
QLineEdit:disabled {
    color: #555b64;
}

/* ---------- Buttons ---------- */
QPushButton#ghost {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 6px;
    color: #9aa1ab;
    font-size: 11px;
    padding: 4px 10px;
}
QPushButton#ghost:hover {
    background: #1a1e23;
    border-color: #282d34;
    color: #e8ebf0;
}
QPushButton#ghost:pressed {
    background: #15181c;
}
QPushButton#ghost:disabled {
    color: #4c525a;
}
QPushButton#secondary {
    background: #171a1f;
    border: 1px solid #282d34;
    border-radius: 6px;
    color: #c2c8d1;
    font-size: 11px;
    padding: 5px 12px;
}
QPushButton#secondary:hover {
    background: #1d2127;
    border-color: #343a43;
    color: #ffffff;
}
QPushButton#secondary:pressed {
    background: #131619;
}
QPushButton#secondary:disabled {
    color: #4c525a;
    border-color: #1f2329;
}
QPushButton#primary {
    background: %ACCENT%;
    border: 1px solid %ACCENT%;
    border-radius: 6px;
    color: %ACCENT_CONTRAST%;
    font-size: 11px;
    font-weight: 600;
    padding: 5px 16px;
}
QPushButton#primary:hover {
    background: %ACCENT_HOVER%;
    border-color: %ACCENT_HOVER%;
}
QPushButton#primary:pressed {
    background: %ACCENT_DIM%;
}
QPushButton#primary:disabled {
    background: #1d2127;
    border-color: #23282f;
    color: #555b64;
}
QPushButton#danger {
    background: transparent;
    border: 1px solid #3a2226;
    border-radius: 6px;
    color: #c98b8b;
    font-size: 11px;
    padding: 5px 12px;
}
QPushButton#danger:hover {
    background: #241619;
    border-color: #5a2f34;
    color: #f0a5a5;
}
QPushButton#rowAction {
    background: transparent;
    border: none;
    border-radius: 4px;
    color: #6f767f;
    font-size: 11px;
    padding: 1px 6px;
}
QPushButton#rowAction:hover {
    background: #1d2127;
    color: %ACCENT%;
}
QPushButton#rowAction:disabled {
    color: #464c54;
}

/* ---------- Chips ---------- */
QLabel#chip {
    color: #9aa1ab;
    background: #171a1f;
    border: 1px solid #262b32;
    border-radius: 4px;
    font-size: 10px;
    padding: 1px 6px;
}
QLabel#chipAccent {
    color: %ACCENT%;
    background: %ACCENT_DIM%;
    border: 1px solid %ACCENT_DIM%;
    border-radius: 4px;
    font-size: 10px;
    padding: 1px 6px;
}
QLabel#chipWarn {
    color: #d8b479;
    background: #211c12;
    border: 1px solid #3a301c;
    border-radius: 4px;
    font-size: 10px;
    padding: 1px 6px;
}
QLabel#chipMissing {
    color: #cd8f8f;
    background: #241518;
    border: 1px solid #3c2226;
    border-radius: 4px;
    font-size: 10px;
    padding: 1px 6px;
}

/* ---------- Filter chips ---------- */
QPushButton#filterChip {
    background: #14171b;
    border: 1px solid #24292f;
    border-radius: 10px;
    color: #828a94;
    font-size: 10px;
    padding: 2px 9px;
}
QPushButton#filterChip:hover {
    border-color: #333a43;
    color: #c9ccd2;
}
QPushButton#filterChip:checked {
    background: %ACCENT_DIM%;
    border-color: %ACCENT%;
    color: %ACCENT%;
}

/* ---------- Lists ---------- */
QListWidget#sessionList {
    background: transparent;
    border: none;
    outline: none;
}
QListWidget#sessionList::item {
    background: #13161a;
    border: 1px solid #20242a;
    border-radius: 7px;
    margin: 0 0 6px 0;
    padding: 0;
    color: #c2c8d1;
}
QListWidget#sessionList::item:hover {
    background: #171b20;
    border-color: #2b313a;
}
QListWidget#sessionList::item:selected {
    background: #171b20;
    border-color: %ACCENT%;
}

/* ---------- Scrollbars ---------- */
QScrollBar:vertical {
    background: transparent;
    width: 9px;
    margin: 2px 1px 2px 1px;
}
QScrollBar::handle:vertical {
    background: #2a3038;
    border-radius: 4px;
    min-height: 26px;
}
QScrollBar::handle:vertical:hover {
    background: #3a424d;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0;
}
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
    background: transparent;
}
QScrollBar:horizontal {
    background: transparent;
    height: 9px;
    margin: 1px 2px 1px 2px;
}
QScrollBar::handle:horizontal {
    background: #2a3038;
    border-radius: 4px;
    min-width: 26px;
}
QScrollBar::handle:horizontal:hover {
    background: #3a424d;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0;
}
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
    background: transparent;
}
QScrollArea#clean, QScrollArea#clean > QWidget > QWidget {
    background: transparent;
    border: none;
}

/* ---------- Menus ---------- */
QMenu {
    background: #16191e;
    border: 1px solid #2a2f37;
    border-radius: 7px;
    padding: 4px;
    color: #c9ccd2;
}
QMenu::item {
    background: transparent;
    border-radius: 5px;
    color: #c9ccd2;
    font-size: 11px;
    padding: 5px 26px 5px 26px;
}
QMenu::item:selected {
    background: #22272e;
    color: #ffffff;
}
QMenu::item:disabled {
    color: #545a63;
}
QMenu::separator {
    background: #262b32;
    height: 1px;
    margin: 4px 8px;
}
QMenu::indicator {
    width: 13px;
    height: 13px;
    left: 7px;
}
QMenu::indicator:checked {
    background: %ACCENT%;
    border: 1px solid %ACCENT%;
    border-radius: 3px;
}

/* ---------- Dialogs ---------- */
QDialog {
    background: #13161a;
}
QDialog QLabel {
    color: #c9ccd2;
    font-size: 11px;
}
QMessageBox, QInputDialog, QFileDialog {
    background: #13161a;
}
QMessageBox QLabel {
    color: #c9ccd2;
    font-size: 11px;
}
QMessageBox QLabel#qt_msgbox_label {
    color: #e4e7ec;
    min-width: 300px;
}
QMessageBox QPushButton, QInputDialog QPushButton {
    background: #171a1f;
    border: 1px solid #282d34;
    border-radius: 6px;
    color: #c2c8d1;
    font-size: 11px;
    padding: 4px 14px;
    min-height: 20px;
}
QMessageBox QPushButton:hover, QInputDialog QPushButton:hover {
    background: #1d2127;
    border-color: #343a43;
    color: #ffffff;
}
QInputDialog QLineEdit {
    background: #0e1013;
    border: 1px solid #262b32;
    border-radius: 6px;
    color: #e4e7ec;
    font-size: 11px;
    padding: 4px 8px;
}
QCheckBox {
    color: #b6bcc5;
    font-size: 11px;
    spacing: 7px;
}
QCheckBox::indicator {
    width: 13px;
    height: 13px;
    border: 1px solid #333941;
    border-radius: 3px;
    background: #0e1013;
}
QCheckBox::indicator:hover {
    border-color: %ACCENT%;
}
QCheckBox::indicator:checked {
    background: %ACCENT%;
    border-color: %ACCENT%;
}
)";

    QString sheet = QString::fromUtf8(kTemplate);
    sheet.replace(QLatin1String("%ACCENT_CONTRAST%"), accentContrastName());
    sheet.replace(QLatin1String("%ACCENT_HOVER%"), accentHoverName());
    sheet.replace(QLatin1String("%ACCENT_DIM%"), accentDimName());
    sheet.replace(QLatin1String("%ACCENT%"), accentName());
    return sheet;
}

} // namespace Theme
