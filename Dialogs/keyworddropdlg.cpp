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
    setWindowTitle("File keywords");
    setModal(true);

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *what = new QLabel(
        QString("Filing %1 into <b>%2</b>")
            .arg(moves.size() == 1 ? QString("1 keyword")
                                   : QString("%1 keywords").arg(moves.size()),
                 targetPath.toHtmlEscaped()), this);
    what->setTextFormat(Qt::RichText);
    layout->addWidget(what);

    /*  A TABLE RATHER THAN A SENTENCE, because several keywords dropped on one branch is
        the ordinary case and each of them lands somewhere different. A prose summary of
        five moves is unreadable; five rows are not. */
    QTableWidget *table = new QTableWidget(moves.size(), ColumnCount, this);
    table->setHorizontalHeaderLabels({"Keyword", "", "Becomes", "Images"});
    table->verticalHeader()->hide();
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->horizontalHeader()->setSectionResizeMode(ColFrom, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(ColArrow,
                                                    QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(ColTo, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(ColImages,
                                                    QHeaderView::ResizeToContents);

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
    QPushButton *goBtn = buttons->addButton(
        QString("File %1").arg(total), QDialogButtonBox::AcceptRole);
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
    resize(620, qMin(560, 260 + moves.size() * 24));
}
