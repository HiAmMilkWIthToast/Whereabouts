#pragma once

#include "icons.h"

#include <QColor>
#include <QPushButton>
#include <QString>

class IconTextButton : public QPushButton
{
    Q_OBJECT
public:
    IconTextButton(Icons::Id icon, const QString &text, QWidget *parent = nullptr);

    void setIconId(Icons::Id id);
    Icons::Id iconId() const { return m_icon; }

    QString label() const { return m_label; }
    void setLabel(const QString &text);

    int glyphWidth() const { return kGlyph; }

    QColor inkColor() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:

    QColor textColor() const;

    Icons::Id m_icon;
    QString m_label;
    bool m_hovered = false;

    static constexpr int kGlyph = 13;
    static constexpr int kGap = 6;
};
