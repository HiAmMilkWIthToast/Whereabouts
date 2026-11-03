#pragma once

#include <QLabel>

class ElidedLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ElidedLabel(QWidget *parent = nullptr);

    QString fullText() const { return m_full; }

public slots:
    void setFullText(const QString &text);

protected:
    void paintEvent(QPaintEvent *event) override;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    QString m_full;
};
