#pragma once

#include <QLabel>

// A label that shows as much of its text as fits, ending in an ellipsis when it
// has to. QLabel itself either widens its layout to fit the text or clips it
// mid-character, and neither is what a long path in a narrow header needs.
//
// The full text is kept, so the tooltip and anything asking the label what it
// says get the truth rather than the shortened form.
class ElidedLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ElidedLabel(QWidget *parent = nullptr);

    // The text as written, before any ellipsis.
    QString fullText() const { return m_full; }

public slots:
    void setFullText(const QString &text);

protected:
    void paintEvent(QPaintEvent *event) override;
    // The width the whole text wants. Preferred sizing means the layout hands
    // it that much when there is room, and takes it away again when there is
    // not; minimumSizeHint() is the floor it can be squeezed to.
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    QString m_full;
};
