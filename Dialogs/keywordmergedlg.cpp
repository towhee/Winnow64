#include "Dialogs/keywordmergedlg.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

/*  The created-node list, as the dialog shows it. CAPPED, because filing a legacy branch
    can add dozens and a dialog that grows past the screen cannot be read or dismissed --
    the same reason KeywordTree::importLightroom lists eight and counts the rest. */
QString createsText(const QStringList &nodes, int alreadyThere)
{
/*
    EVERY MOVING KEYWORD IS ACCOUNTED FOR, not just the ones being created. A list of six
    under a heading that said nineteen were moving read as thirteen missing branches --
    reported as a bug on first use, and it was the dialog that was wrong, not the
    arithmetic. The ones not listed are not missing; they are already in the keyword list,
    which is the ordinary case and the good one.
*/
    if (nodes.isEmpty())
        return alreadyThere == 1
            ? QString("Your keyword list already has it, so nothing will be added to it.")
            : QString("Your keyword list already has all %1 of them, so nothing will be "
                      "added to it.").arg(alreadyThere);

    /*  "KEYWORD BRANCHES (NODES)", NOT "KEYWORDS". Three different things in this dialog
        are counted in keywords -- the ones moving, the ones the list already has, and the
        entries about to be created -- and calling the last of those "keywords" left the
        reader comparing it against the image count on the buttons. A node is what is
        actually being added, and naming it that is what keeps the three apart. */
    QString s;
    if (alreadyThere > 0)
        s = QString("Your keyword list already has %1 of them. This will add %2 keyword "
                    "branch%3 (node%4) to your list:\n    %5")
                .arg(alreadyThere).arg(nodes.size())
                .arg(nodes.size() == 1 ? "" : "es").arg(nodes.size() == 1 ? "" : "s")
                .arg(nodes.mid(0, 8).join("\n    "));
    else
        s = QString("This will add %1 keyword branch%2 (node%3) to your list:\n    %4")
                .arg(nodes.size())
                .arg(nodes.size() == 1 ? "" : "es").arg(nodes.size() == 1 ? "" : "s")
                .arg(nodes.mid(0, 8).join("\n    "));
    if (nodes.size() > 8) s += "\n    ...";
    return s;
}

}   // namespace

KeywordMergeDlg::KeywordMergeDlg(const QString &unfiledPath, int observedCount,
                                 const QList<KeywordMergeTarget> &targets,
                                 int folderCount, int catalogCount,
                                 bool folderScope, const QString &folderName,
                                 QWidget *parent)
    : QDialog(parent), targets_(targets)
{
    setWindowTitle("Merge into keyword list");
    setModal(true);

    QVBoxLayout *layout = new QVBoxLayout(this);

    /*  ONE OPENING SENTENCE, AND IT NAMES WHAT MOVES.

        It used to open with "USA is not in your keyword list", which is true and is not
        the point: the user right-clicked a keyword drawn in the unfiled colour, so being
        unfiled is what they already know and is why they are here. Saying it back reads
        as the dialog's subject, and it pushed the one fact this dialog can give that the
        panel behind it cannot -- that the whole BRANCH travels with the node that was
        clicked -- into a second sentence. Reported as misleading on first use. */
    const QString name = unfiledPath.toHtmlEscaped();
    QLabel *what = new QLabel(
        observedCount > 1
            ? QString("<b>%1</b> and its %2 keyword%3 beneath it will be moved together, "
                      "keeping their own branches.")
                  .arg(name).arg(observedCount - 1).arg(observedCount == 2 ? "" : "s")
            : QString("<b>%1</b> will be moved into your keyword list.").arg(name),
        this);
    what->setTextFormat(Qt::RichText);
    what->setWordWrap(true);
    layout->addWidget(what);
    layout->addSpacing(6);

    /*  ONE CANDIDATE IS STATED; SEVERAL ARE ASKED. A list with a single row is a question
        with one answer, which reads as a decision the user has to make and is not one. */
    if (targets_.size() == 1) {
        QLabel *into = new QLabel(
            QString("Merge it into <b>%1</b>.").arg(targets_.at(0).path.toHtmlEscaped()),
            this);
        into->setTextFormat(Qt::RichText);
        into->setWordWrap(true);
        layout->addWidget(into);
    }
    else {
        QLabel *pick = new QLabel(
            QString("Your keyword list has %1 branches with that name. Merge it into:")
                .arg(targets_.size()), this);
        pick->setWordWrap(true);
        layout->addWidget(pick);

        list_ = new QListWidget(this);
        for (const KeywordMergeTarget &t : targets_) {
            /*  THE COUNT IS WHAT TELLS TWO SAME-NAMED BRANCHES APART. The paths differ
                only in their parents, which is exactly the information the user is short
                of; how many photographs are already filed under each is usually the
                whole answer. */
            list_->addItem(QString("%1        (%2 image%3)")
                               .arg(t.path)
                               .arg(QLocale().toString(t.images))
                               .arg(t.images == 1 ? "" : "s"));
        }
        list_->setCurrentRow(0);
        layout->addWidget(list_);
        connect(list_, &QListWidget::currentRowChanged,
                this, &KeywordMergeDlg::showCreatesFor);
    }

    creates_ = new QLabel(this);
    creates_->setWordWrap(true);
    layout->addWidget(creates_);
    showCreatesFor(0);

    /*  THE SCOPE PARAGRAPH, for the same reason KeywordDropDlg carries one: the two
        choices below do not touch the same set of files, and only one of them is bounded
        by what happens to be open. Unlike the drag, this operation is driven from the
        CATALOG rather than from loaded rows, so "Everywhere" really does reach folders
        that were never opened -- which is the reason to prefer it and the reason to say
        which one is which. */
    if (folderScope && !folderName.isEmpty()) {
        QLabel *scope = new QLabel(
            QString("You are browsing \"%1\". \"Everywhere\" still rewrites every image "
                    "in the catalog that carries this keyword, including folders that "
                    "are not open.").arg(folderName), this);
        scope->setWordWrap(true);
        layout->addWidget(scope);
    }

    QLabel *note = new QLabel(
        "The keywords are rewritten inside the images' own sidecar files. This cannot be "
        "undone.", this);
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addSpacing(6);

    QDialogButtonBox *buttons = new QDialogButtonBox(this);

    /*  THE BUTTONS SAY WHAT THEY ARE COUNTING. Three numbers appear in this dialog --
        photographs to rewrite, keywords moving, keyword-list entries to add -- and the
        one on the buttons was the only bare one, so it was read as the others. "images"
        costs six characters and removes the ambiguity entirely. */
    QPushButton *folderBtn = buttons->addButton(
        QString("This folder  (%1 images)").arg(QLocale().toString(folderCount)),
        QDialogButtonBox::ActionRole);
    folderBtn->setEnabled(folderCount > 0);

    QPushButton *allBtn = buttons->addButton(
        QString("Everywhere  (%1 images)").arg(QLocale().toString(catalogCount)),
        QDialogButtonBox::ActionRole);
    allBtn->setEnabled(catalogCount > 0);

    QPushButton *notNowBtn = buttons->addButton("Not now", QDialogButtonBox::RejectRole);

    /*  Not now is the default, deliberately -- the same rule KeywordRetagDlg states.
        Return should not rewrite files, and the safe choice is the one a user who has
        stopped reading will take. */
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

void KeywordMergeDlg::showCreatesFor(int row)
{
    if (row < 0 || row >= targets_.size()) return;
    if (creates_ != nullptr)
        creates_->setText(createsText(targets_.at(row).createsNodes,
                                      targets_.at(row).alreadyThere));
}

QString KeywordMergeDlg::target() const
{
    const int row = list_ != nullptr ? list_->currentRow() : 0;
    if (row < 0 || row >= targets_.size()) return QString();
    return targets_.at(row).path;
}
