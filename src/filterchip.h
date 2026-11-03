#pragma once

#include <QColor>
#include <QPushButton>
#include <QString>

class FilterChip : public QPushButton
{
    Q_OBJECT
public:
    FilterChip(const QString &label, int count, QWidget *parent = nullptr);

    QString label() const { return m_label; }
    int count() const { return m_count; }

    QSize sizeHint() const override;

    int leftInset() const;
    int rightInset() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void layoutText() const;

    int contentWidth() const;

    QString m_label;
    int m_count = 0;
    QColor m_countColor;

    mutable int m_labelAdvance = 0;
    mutable int m_countAdvance = 0;
    mutable int m_countOriginX = 0;

    mutable int m_runLeft = 0;
    mutable int m_runWidth = 0;
    static constexpr int kGap = 5;
    static constexpr int kPadX = 9;
};
