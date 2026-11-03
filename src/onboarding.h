#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QLineEdit;
class QPushButton;
class QCheckBox;

class OnboardingDialog : public QDialog
{
    Q_OBJECT
public:
    explicit OnboardingDialog(const QString &suggestedPath, bool firstRun,
                              QWidget *parent = nullptr);

    QString chosenPath() const;
    QString sessionName() const;

    bool openInNewWindow() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void browse();
    void validate();
    void acceptCapture();

    QLabel *m_headline = nullptr;
    QLabel *m_body = nullptr;
    QLineEdit *m_pathEdit = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QLabel *m_pathStatus = nullptr;
    QPushButton *m_capture = nullptr;
    QCheckBox *m_newWindow = nullptr;
    bool m_firstRun = true;
};
