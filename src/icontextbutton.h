#pragma once

#include "icons.h"

#include <QColor>
#include <QPushButton>
#include <QString>

// A push button with a glyph before its label.
//
// QSS cannot tint a QIcon, so a button styled entirely in the stylesheet would
// either ship a glyph frozen at one colour or need the icon swapped by hand on
// every hover. This paints instead and reads its colours from the same rules the
// stylesheet uses, so the glyph changes with the text and the button still
// looks like the plain ones beside it.
//
// It stays a QPushButton so focus, keyboard activation, the default-button ring
// and setEnabled() keep working without being reimplemented.
class IconTextButton : public QPushButton
{
    Q_OBJECT
public:
    IconTextButton(Icons::Id icon, const QString &text, QWidget *parent = nullptr);

    void setIconId(Icons::Id id);
    Icons::Id iconId() const { return m_icon; }

    // The painted label. setText() is emptied deliberately, so this is the only
    // way to ask what the button actually draws.
    QString label() const { return m_label; }
    void setLabel(const QString &text);

    // Gap between the glyph and its label, exposed so a check can assert the
    // two do not overlap at any size.
    int glyphWidth() const { return kGlyph; }

    // The colour both the glyph and the label are painted with, resolved from
    // the stylesheet for the current state. Public so a check can assert that
    // hover actually changes what is drawn, without sampling rendered pixels
    // (which always include the background the stylesheet repaints anyway).
    QColor inkColor() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    // Both taken from the stylesheet rule for this button's object name, so the
    // palette in the QSS remains the single source of truth for colour.
    QColor textColor() const;

    Icons::Id m_icon;
    QString m_label;
    bool m_hovered = false;

    static constexpr int kGlyph = 13;   // logical px, matches the icon set
    static constexpr int kGap = 6;      // between glyph and text
};
