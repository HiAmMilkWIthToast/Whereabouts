#include "onboarding.h"
#include "icons.h"
#include "sessionstore.h"
#include "theme.h"

#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windows.h>
#endif

namespace {

QLabel *label(const QString &text, const QString &objectName, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setObjectName(objectName);
    l->setWordWrap(true);
    return l;
}

}

OnboardingDialog::OnboardingDialog(const QString &suggestedPath, bool firstRun,
                                   QWidget *parent)
    : QDialog(parent), m_firstRun(firstRun)
{
    setWindowTitle(firstRun ? QStringLiteral("Welcome to Cairn")
                            : QStringLiteral("New snapshot"));
    setObjectName(QStringLiteral("onboarding"));
    setWindowFlag(Qt::FramelessWindowHint, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setModal(true);
    setFixedWidth(470);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 20);
    root->setSpacing(0);

    auto *head = new QHBoxLayout;
    head->setSpacing(11);

    auto *mark = new QLabel(this);
    mark->setFixedSize(30, 30);
    mark->setPixmap(Icons::render(Icons::Id::Cairn, 28, Theme::brand(),
                                  devicePixelRatioF()));
    head->addWidget(mark);

    auto *headText = new QVBoxLayout;
    headText->setSpacing(1);

    m_headline = label(firstRun ? QStringLiteral("Snapshot a folder")
                                : QStringLiteral("New snapshot"),
                       QStringLiteral("pageTitle"), this);
    headText->addWidget(m_headline);

    m_body = label(
        firstRun
            ? QStringLiteral("Cairn records where everything sits in a folder — "
                             "position, type, size — and writes it to a small "
                             ".session file. Nothing is copied or backed up.")
            : QStringLiteral("Record the current layout of a folder."),
        QStringLiteral("hint"), this);
    headText->addWidget(m_body);

    head->addLayout(headText, 1);
    root->addLayout(head);
    root->addSpacing(18);

    root->addWidget(label(QStringLiteral("FOLDER TO SNAPSHOT"),
                          QStringLiteral("fieldLabel"), this));
    root->addSpacing(5);

    auto *pathRow = new QHBoxLayout;
    pathRow->setSpacing(6);

    m_pathEdit = new QLineEdit(suggestedPath, this);
    m_pathEdit->setObjectName(QStringLiteral("monoField"));
    m_pathEdit->setClearButtonEnabled(false);
    pathRow->addWidget(m_pathEdit, 1);

    auto *browse = new QPushButton(QStringLiteral("Browse"), this);
    browse->setObjectName(QStringLiteral("secondary"));
    browse->setCursor(Qt::PointingHandCursor);
    pathRow->addWidget(browse);
    root->addLayout(pathRow);

    root->addSpacing(6);
    m_pathStatus = label(QString(), QStringLiteral("fieldLabel"), this);
    root->addWidget(m_pathStatus);

    root->addSpacing(14);

    root->addWidget(label(QStringLiteral("SESSION NAME"),
                          QStringLiteral("fieldLabel"), this));
    root->addSpacing(5);
    m_nameEdit = new QLineEdit(QString(), this);
    m_nameEdit->setObjectName(QStringLiteral("field"));
    m_nameEdit->setPlaceholderText(QStringLiteral("Leave blank to use the folder name"));
    root->addWidget(m_nameEdit);

    root->addSpacing(14);

    m_newWindow = new QCheckBox(
        QStringLiteral("Open the snapshot in its own window"), this);
    m_newWindow->setChecked(false);
    root->addWidget(m_newWindow);

    if (firstRun) {
        root->addSpacing(10);
        auto *note = label(
            QStringLiteral("A session file is a map, not a backup: it stores "
                           "positions and metadata, never the files themselves."),
            QStringLiteral("hint"), this);
        root->addWidget(note);
    }

    root->addSpacing(18);

    auto *actions = new QHBoxLayout;
    actions->setSpacing(8);
    actions->addStretch(1);

    auto *cancel = new QPushButton(QStringLiteral("Cancel"), this);
    cancel->setObjectName(QStringLiteral("ghost"));
    cancel->setCursor(Qt::PointingHandCursor);
    actions->addWidget(cancel);

    m_capture = new QPushButton(QStringLiteral("Capture snapshot"), this);
    m_capture->setObjectName(QStringLiteral("primary"));
    m_capture->setCursor(Qt::PointingHandCursor);
    m_capture->setDefault(true);
    actions->addWidget(m_capture);

    root->addLayout(actions);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_capture, &QPushButton::clicked, this, &OnboardingDialog::acceptCapture);
    connect(browse, &QPushButton::clicked, this, &OnboardingDialog::browse);
    connect(m_pathEdit, &QLineEdit::textChanged, this, &OnboardingDialog::validate);
    connect(m_nameEdit, &QLineEdit::returnPressed, this, &OnboardingDialog::acceptCapture);
    connect(m_pathEdit, &QLineEdit::returnPressed, this, &OnboardingDialog::acceptCapture);

    validate();
    m_pathEdit->setFocus();
    m_pathEdit->selectAll();
}

QString OnboardingDialog::chosenPath() const
{
    const QString typed = m_pathEdit->text().trimmed();

    if (typed.isEmpty())
        return QString();
    return QDir::cleanPath(typed);
}

QString OnboardingDialog::sessionName() const
{
    const QString typed = m_nameEdit->text().trimmed();
    return typed.isEmpty() ? SessionStore::suggestedName(chosenPath()) : typed;
}

bool OnboardingDialog::openInNewWindow() const
{
    return m_newWindow->isChecked();
}

void OnboardingDialog::browse()
{
    const QString start = QFileInfo(m_pathEdit->text()).isDir()
        ? m_pathEdit->text()
        : SessionStore::desktopPath();
    const QString path = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Choose a folder to snapshot"), start,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!path.isEmpty())
        m_pathEdit->setText(QDir::toNativeSeparators(path));
}

void OnboardingDialog::validate()
{
    const QString path = chosenPath();
    const QFileInfo info(path);

    if (path.isEmpty()) {
        m_pathStatus->setText(QStringLiteral("Choose a folder."));
        m_pathStatus->setObjectName(QStringLiteral("fieldLabel"));
        m_capture->setEnabled(false);
    } else if (!info.exists() || !info.isDir()) {
        m_pathStatus->setText(QStringLiteral("That folder does not exist."));
        m_pathStatus->setObjectName(QStringLiteral("chipMissing"));
        m_capture->setEnabled(false);
    } else if (SessionStore::isDesktopPath(path)) {
        m_pathStatus->setText(
            QStringLiteral("This is your desktop — real icon positions will be used."));
        m_pathStatus->setObjectName(QStringLiteral("chipAccent"));
        m_capture->setEnabled(true);
    } else {
        m_pathStatus->setText(
            QStringLiteral("Positions will be laid out on a grid."));
        m_pathStatus->setObjectName(QStringLiteral("chip"));
        m_capture->setEnabled(true);
    }

    m_pathStatus->style()->unpolish(m_pathStatus);
    m_pathStatus->style()->polish(m_pathStatus);
}

void OnboardingDialog::acceptCapture()
{
    if (!m_capture->isEnabled())
        return;
    accept();
}

void OnboardingDialog::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(QStringLiteral("#12151a")));
    p.drawRoundedRect(QRectF(rect()).adjusted(0, 0, -1, -1), 11, 11);

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(QStringLiteral("#2a2f37")), 1.0));
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 11, 11);
}
