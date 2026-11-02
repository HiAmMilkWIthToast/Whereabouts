#pragma once

#include <QColor>
#include <QPushButton>
#include <QString>

// A filter pill: a kind's name with its count beside it.
//
// It is painted rather than styled with QSS padding, because the count has to
// read as secondary information. QSS can only style one run of text per button,
// which forced the name and count into one string separated by a space, and the
// padding then had to be guessed to keep the two centred. Painting the two runs
// at measured offsets keeps the label optically centred and the count visibly
// dimmer, and makes the left and right insets identical by construction.
class FilterChip : public QPushButton
{
    Q_OBJECT
public:
    FilterChip(const QString &label, int count, QWidget *parent = nullptr);

    QString label() const { return m_label; }
    int count() const { return m_count; }

    QSize sizeHint() const override;

    // Horizontal insets actually used, exposed so tests can assert that the
    // left and right padding match instead of trusting the paint code.
    int leftInset() const;
    int rightInset() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void layoutText() const;
    // Width the drawn run actually occupies, measured from ink. Both the paint
    // code and the inset accessors use this, so they cannot disagree.
    int contentWidth() const;

    QString m_label;
    int m_count = 0;
    QColor m_countColor;

    // Advance widths, which set the chip's size and the distance between the
    // two runs. They are stable, so sizeHint can rely on them.
    mutable int m_labelAdvance = 0;
    mutable int m_countAdvance = 0;
    mutable int m_countOriginX = 0;

    // The painted extent of the whole run, measured by actually rasterising it.
    // Placing the text by predicting where ink will land was the source of the
    // off-centre complaint; measuring it removes the guess.
    mutable int m_runLeft = 0;
    mutable int m_runWidth = 0;
    static constexpr int kGap = 5;      // between label and count
    static constexpr int kPadX = 9;     // left and right, deliberately equal
};
