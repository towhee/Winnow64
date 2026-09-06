#include "Dialogs/keywordretagdlg.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

KeywordRetagDlg::KeywordRetagDlg(const QString &oldPath, const QString &newPath,
                                 int folderCount, int catalogCount, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Update images?");
    setModal(true);

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *what = new QLabel(
        QString("<b>%1</b><br>is now<br><b>%2</b>").arg(oldPath.toHtmlEscaped(),
                                                        newPath.toHtmlEscaped()), this);
    what->setTextFormat(Qt::RichText);
    layout->addWidget(what);

    /*  The vocabulary has ALREADY changed. Saying so removes the obvious misreading of a
        confirmation dialog -- that cancelling undoes the rename -- and makes the actual
        question ("and the files?") the only one on screen. */
    QLabel *note = new QLabel(
        "Your keyword list has been updated. The images already tagged with it still "
        "carry the old keyword in their own files.", this);
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addSpacing(6);

    QDialogButtonBox *buttons = new QDialogButtonBox(this);

    QPushButton *folderBtn = buttons->addButton(
        QString("This folder  (%1)").arg(folderCount), QDialogButtonBox::ActionRole);
    folderBtn->setEnabled(folderCount > 0);

    QPushButton *allBtn = buttons->addButton(
        QString("Everywhere  (%1)").arg(catalogCount), QDialogButtonBox::ActionRole);
    allBtn->setEnabled(catalogCount > 0);

    QPushButton *notNowBtn = buttons->addButton("Not now", QDialogButtonBox::RejectRole);

    /*  "Not now" is the default, deliberately. Return should not rewrite files, and the
        safe choice is the one a user who has stopped reading will take. */
    notNowBtn->setDefault(true);
    notNowBtn->setAutoDefault(true);

    connect(folderBtn, &QPushButton::clicked, this, [this]{
        result_ = ThisFolder;
        accept();
    });
    connect(allBtn, &QPushButton::clicked, this, [this]{
        result_ = Everywhere;
        accept();
    });
    connect(notNowBtn, &QPushButton::clicked, this, [this]{
        result_ = NotNow;
        reject();
    });

    layout->addWidget(buttons);
}
