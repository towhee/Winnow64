#include "Dialogs/keyworddropdlg.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

enum Column { ColFrom = 0, ColArrow, ColTo, ColImages, ColumnCount };

}   // namespace

KeywordDropDlg::KeywordDropDlg(const QString &targetPath, const QList<KeywordMove> &moves,
                               const QStringList &skipped, bool folderScope,
                               const QString &folderName, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Sync image and keyword list keywords");
    setModal(true);

    QVBoxLayout *layout = new QVBoxLayout(this);

    /*  NO "FILING 1 KEYWORD INTO X" LINE. It said what the table below says, in worse
        detail: the table names every keyword, its destination and its count, so a
        sentence summarising it is one more thing to read before the answer.
*/
    /*  A TABLE RATHER THAN A SENTENCE, because several keywords dropped on one branch is
        the ordinary case and each of them lands somewhere different. A prose summary of
        five moves is unreadable; five rows are not. */
    QTableWidget *table = new QTableWidget(moves.size(), ColumnCount, this);
    table->setHorizontalHeaderLabels({"Keyword", "", "Becomes", "Images"});
    table->verticalHeader()->hide();
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);

    /*  INTERACTIVE, NOT STRETCHED. Keyword paths are long and their lengths are nothing
        like each other -- "Animals" beside "Fauna|Bird|Ferruginous Hawk" -- so a split
        that suits one row elides the next. Stretch mode also refuses to be dragged at
        all, which is what made this unreadable: the column you needed wider was the one
        Qt would not let you widen. The initial widths still come from the CONTENT
        (resizeColumnsToContents below, then switched to Interactive), so the common case
        needs no dragging and the uncommon one is possible. */
    QHeaderView *hh = table->horizontalHeader();
    hh->setSectionResizeMode(QHeaderView::Interactive);
    hh->setStretchLastSection(false);

    int total = 0;
    int creating = 0;
    for (int i = 0; i < moves.size(); ++i) {
        const KeywordMove &m = moves.at(i);
        total += m.images;
        if (m.createsNode) ++creating;

        table->setItem(i, ColFrom, new QTableWidgetItem(m.from));
        table->setItem(i, ColArrow, new QTableWidgetItem(QString(QChar(0x2192))));
        /*  "(new)" ON THE ROW THAT CREATES IT, not only in the summary below. The summary
            says how many nodes are being added; only the row says WHICH, and which is
            what a user checks before letting something write to their keyword list. */
        table->setItem(i, ColTo, new QTableWidgetItem(
            m.createsNode ? QString("%1   (new)").arg(m.to) : m.to));
        QTableWidgetItem *n = new QTableWidgetItem(QString::number(m.images));
        n->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(i, ColImages, n);
    }
    layout->addWidget(table);

    QStringList lines;
    lines << QString("%1 image%2 will be rewritten. This cannot be undone.")
                 .arg(total).arg(total == 1 ? "" : "s");
    if (creating > 0) {
        lines << QString("%1 new keyword%2 will be added to your keyword list.")
                     .arg(creating).arg(creating == 1 ? "" : "s");
    }
    if (!skipped.isEmpty()) {
        /*  NAMED, NOT SILENTLY DROPPED. The user checked these rows; a dialog that simply
            showed fewer moves than they had checked would look like it had lost some. */
        lines << QString("Not moved: %1.").arg(skipped.join("; "));
    }

    QLabel *summary = new QLabel(lines.join("\n"), this);
    summary->setWordWrap(true);
    layout->addWidget(summary);

    /*  THE SCOPE LIMIT, WHICH IS INVISIBLE EVERYWHERE ELSE. A drop reaches only the
        images the datamodel has loaded, so in Folders scope every other folder keeps the
        keyword and the panel gives no hint of it. */
    if (folderScope) {
        QLabel *scope = new QLabel(
            QString("Scope: Folders%1. Images in other folders keep these keywords. "
                    "Switch the Filters panel to Catalog scope to file them everywhere.")
                .arg(folderName.isEmpty() ? QString()
                                          : QString(" (%1)").arg(folderName)), this);
        scope->setWordWrap(true);
        layout->addWidget(scope);
    }

    layout->addSpacing(6);

    QDialogButtonBox *buttons = new QDialogButtonBox(this);
    /*  THE BUTTON NAMES THE CONSEQUENCE, not the count. "File 62" reads as a quantity
        of nothing in particular; what the press actually does is rewrite the keywords
        inside the user's image files, which is the fact worth having under the cursor.
        The count is two lines above it. */
    QPushButton *goBtn = buttons->addButton(
        "Update image file(s) keywords", QDialogButtonBox::AcceptRole);
    QPushButton *cancelBtn = buttons->addButton(QDialogButtonBox::Cancel);

    goBtn->setEnabled(total > 0);
    /*  Cancel is the default, for the reason KeywordRetagDlg gives: Return must not
        rewrite files, and the safe choice is the one a user who has stopped reading
        takes. */
    cancelBtn->setDefault(true);
    cancelBtn->setAutoDefault(true);

    connect(goBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    layout->addWidget(buttons);

    /*  Content widths first, THEN Interactive: sizing a section after the mode is set
        keeps the width Qt calculated while leaving the handle draggable. */
    table->resizeColumnsToContents();
    resize(680, qMin(560, 280 + moves.size() * 24));
}
