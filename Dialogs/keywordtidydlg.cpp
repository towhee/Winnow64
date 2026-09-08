#include "Dialogs/keywordtidydlg.h"

#include "Main/global.h"
#include "Metadata/keywordpaths.h"

#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

/* The row reads left to right as the sentence it is: this keyword, on this many images,
   is moved / removed / left alone, to there, and here is what it matched. */
enum Column { ColKeyword = 0, ColImages = 1, ColAction = 2, ColTarget = 3, ColMatch = 4,
              ColCount = 5 };

/* What a row will do. The order is the order in the combo, and Move is first because it
   is the operation the dialog exists for. */
enum Action { Move = 0, Remove = 1, Skip = 2 };

const char *kActionText[] = { "Move to", "Remove", "Leave alone" };

/* What the Show list restricts the table to. The first four are about what a row MATCHED
   -- the fact the user is working through -- and the last three about what it will DO,
   which is how the plan is checked before it is applied. */
enum Show { ShowAll = 0, ShowUnique, ShowReview, ShowNoMatch,
            ShowWillMove, ShowWillRemove, ShowSkipped };

/* Per-row payload, on the keyword item so a sort cannot separate it from its row. */
enum DataRole {
    CandidatesRole = Qt::UserRole + 1,    // QStringList, deepest first
    ReviewRole                            // bool: more than one candidate
};

/*
    ONE DELEGATE FOR BOTH EDITABLE COLUMNS, because they are the same widget with
    different contents and a second class would only duplicate the commit plumbing.

    THE TARGET COMBO IS EDITABLE AND COMPLETES OVER THE WHOLE VOCABULARY. The candidates
    are what the leaf name matched, and they are usually right, but the whole point of a
    tidy is that some keyword belongs somewhere its name does not say -- "Piper's" under
    Location|Canada|BC|Vancouver Island|Nanaimo|Piper's Lagoon. Restricting the choice to
    the matches would make those rows unfixable without leaving the dialog.
*/
class ComboDelegate : public QStyledItemDelegate
{
public:
    ComboDelegate(const QStringList &allPaths, QObject *parent = nullptr)
        : QStyledItemDelegate(parent), allPaths(allPaths) {}

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &idx) const override
    {
        QComboBox *cb = new QComboBox(parent);
        if (idx.column() == ColAction) {
            for (const char *t : kActionText) cb->addItem(t);
            cb->setCurrentIndex(idx.data(Qt::UserRole).toInt());
            return cb;
        }
        cb->setEditable(true);
        cb->addItems(idx.data(Qt::UserRole).toStringList());
        QCompleter *c = new QCompleter(allPaths, cb);
        c->setCaseSensitivity(Qt::CaseInsensitive);
        /* Contains, not starts-with: the user knows the leaf, not the root it hangs
           under -- typing "nanaimo" has to reach Location|Canada|BC|...|Nanaimo. */
        c->setFilterMode(Qt::MatchContains);
        c->setCompletionMode(QCompleter::PopupCompletion);
        cb->setCompleter(c);
        cb->setCurrentText(idx.data(Qt::EditRole).toString());
        return cb;
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &idx) const override
    {
        QComboBox *cb = qobject_cast<QComboBox *>(editor);
        if (!cb) return;
        if (idx.column() == ColAction) model->setData(idx, cb->currentIndex(),
                                                      Qt::UserRole);
        else model->setData(idx, cb->currentText().trimmed(), Qt::EditRole);
    }

private:
    QStringList allPaths;
};

}  // namespace

KeywordTidyDlg::KeywordTidyDlg(const QList<CatalogKeyword> &flat,
                               const QStringList &vocabPaths,
                               const QHash<QString, int> &pathCounts, QWidget *parent)
    : QDialog(parent)
{
    if (G::isLogger) G::log("KeywordTidyDlg::KeywordTidyDlg");

    setWindowTitle("Tidy Flat Keywords");
    setModal(true);
    setMinimumSize(880, 560);

    allPaths = vocabPaths;
    counts = pathCounts;
    for (const QString &p : vocabPaths) {
        if (!p.contains('|')) continue;         // a root is not somewhere to move a root
        byLeafFold[keywordFold(keywordLeafOf(p))] << p;
    }

    QLabel *intro = new QLabel(
        "These keywords sit on their own, outside your keyword list's branches -- the "
        "way keywords arrive from an application that never wrote a hierarchy.\n\n"
        "Every row carries an ACTION, and Apply carries out all of them -- not just the "
        "rows you have selected. Set them with the buttons below the list, or one row "
        "at a time in the Action column.\n\n"
        "Moving a keyword rewrites it in every image that carries it, everywhere in the "
        "catalog, and the images need not be open. Rows that matched more than one "
        "branch are marked \"review\": the deepest match is chosen for you, and it is "
        "not always the right one -- hover the match to see the alternatives with the "
        "number of images each already holds.", this);
    intro->setWordWrap(true);

    filterEdit = new QLineEdit(this);
    filterEdit->setPlaceholderText("Find keyword");
    filterEdit->setClearButtonEnabled(true);

    /*  THE SHOW LIST IS HOW A WHOLE CLASS OF ROWS IS DEALT WITH AT ONCE. "Move every
        keyword that matched exactly one branch" is the operation this dialog is for, and
        the first version could not express it: the only filter was "needs review", its
        own inverse. Narrow the list, then press a button -- the buttons act on what is
        shown. The order is the order of confidence, safest first. */
    showCombo = new QComboBox(this);
    showCombo->addItem("All keywords", int(ShowAll));
    showCombo->addItem("Matched one branch", int(ShowUnique));
    showCombo->addItem("Matched several -- review", int(ShowReview));
    showCombo->addItem("Matched nothing", int(ShowNoMatch));
    showCombo->addItem("Will move", int(ShowWillMove));
    showCombo->addItem("Will be removed", int(ShowWillRemove));
    showCombo->addItem("Left alone", int(ShowSkipped));

    QHBoxLayout *findRow = new QHBoxLayout;
    findRow->setContentsMargins(0, 0, 0, 0);
    findRow->addWidget(filterEdit, 1);
    findRow->addWidget(new QLabel("Show:", this));
    findRow->addWidget(showCombo);

    table = new QTableWidget(0, ColCount, this);
    table->setHorizontalHeaderLabels(QStringList()
        << "Flat keyword" << "Images" << "Action" << "Goes to" << "Match");
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::AllEditTriggers);
    table->setItemDelegateForColumn(ColAction, new ComboDelegate(allPaths, this));
    table->setItemDelegateForColumn(ColTarget, new ComboDelegate(allPaths, this));

    QHeaderView *hh = table->horizontalHeader();
    hh->setSectionResizeMode(ColKeyword, QHeaderView::Interactive);
    hh->setSectionResizeMode(ColImages, QHeaderView::Fixed);
    hh->setSectionResizeMode(ColAction, QHeaderView::Fixed);
    /* The path is the long one and the one worth reading, so it takes what is left. */
    hh->setSectionResizeMode(ColTarget, QHeaderView::Stretch);
    hh->setSectionResizeMode(ColMatch, QHeaderView::ResizeToContents);
    {
        QFontMetrics fm(table->font());
        table->setColumnWidth(ColKeyword, fm.horizontalAdvance("Little Qualicum Estuary")
                                              + 24);
        table->setColumnWidth(ColImages, fm.horizontalAdvance("Images") + 28);
        table->setColumnWidth(ColAction, fm.horizontalAdvance("Leave alone") + 40);
    }

    for (const CatalogKeyword &k : flat) addRow(k);

    /* Sorting is on AFTER the rows are in: with it on during the fill, every insert
       re-sorts the table and the row indexes move under the code doing the filling. */
    table->setSortingEnabled(true);

    QPushButton *setMove = new QPushButton("Move", this);
    setMove->setToolTip("File each of these under the branch in its \"Goes to\" column. "
                        "A row that matched nothing is left as it is.");
    QPushButton *setRemove = new QPushButton("Remove", this);
    setRemove->setToolTip("Take these keywords off the images that carry them.");
    QPushButton *setSkip = new QPushButton("Leave alone", this);

    scopeHint = new QLabel(this);

    QHBoxLayout *bulk = new QHBoxLayout;
    bulk->setContentsMargins(0, 0, 0, 0);
    bulk->addWidget(setMove);
    bulk->addWidget(setRemove);
    bulk->addWidget(setSkip);
    bulk->addWidget(scopeHint, 1);

    summary = new QLabel(this);
    summary->setWordWrap(true);

    QDialogButtonBox *buttons = new QDialogButtonBox(this);
    applyBtn = buttons->addButton("Apply", QDialogButtonBox::AcceptRole);
    QPushButton *cancel = buttons->addButton("Cancel", QDialogButtonBox::RejectRole);
    /* Cancel is the default: Return in a table full of combos must not start rewriting
       thousands of files. */
    cancel->setDefault(true);
    cancel->setAutoDefault(true);
    applyBtn->setAutoDefault(false);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addLayout(findRow);
    layout->addWidget(table, 1);
    layout->addLayout(bulk);
    layout->addWidget(summary);
    layout->addWidget(buttons);

    connect(filterEdit, &QLineEdit::textChanged, this, [this]{ applyFilter(); });
    connect(showCombo, &QComboBox::currentIndexChanged, this, [this]{ applyFilter(); });
    connect(setMove, &QPushButton::clicked, this,
            [this]{ setRows(targetRows(), Move); });
    connect(setRemove, &QPushButton::clicked, this,
            [this]{ setRows(targetRows(), Remove); });
    connect(setSkip, &QPushButton::clicked, this,
            [this]{ setRows(targetRows(), Skip); });
    connect(table->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this]{ updateScopeHint(); });
    connect(table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item){
        if (!item) return;
        /*  Choosing a branch by hand IS choosing to move: a user who types a path into a
            row left at Remove and then presses Apply has said what they want twice, and
            making them also change the action would be pedantry. */
        if (item->column() == ColTarget && !item->text().isEmpty()) {
            QTableWidgetItem *act = table->item(item->row(), ColAction);
            if (act && act->data(Qt::UserRole).toInt() != Move) {
                act->setData(Qt::UserRole, int(Move));
                act->setText(kActionText[Move]);
            }
        }
        if (item->column() == ColAction) {
            item->setText(kActionText[item->data(Qt::UserRole).toInt()]);
            QTableWidgetItem *tgt = table->item(item->row(), ColTarget);
            /*  Remove and Leave alone have nowhere to go, so the target is greyed rather
                than cleared: the match it found is still worth reading, and coming back
                to Move must not have thrown the choice away. */
            if (tgt) tgt->setForeground(
                item->data(Qt::UserRole).toInt() == Move
                    ? table->palette().text() : table->palette().brush(QPalette::Disabled,
                                                                       QPalette::Text));
        }
        if (!bulkEdit) updateSummary();
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    applyFilter();
    updateSummary();
}

void KeywordTidyDlg::addRow(const CatalogKeyword &k)
{
    const QStringList cands = candidatesFor(k.path);
    const int row = table->rowCount();
    table->insertRow(row);

    QTableWidgetItem *name = new QTableWidgetItem(k.path);
    name->setFlags(name->flags() & ~Qt::ItemIsEditable);
    name->setData(CandidatesRole, cands);
    name->setData(ReviewRole, cands.size() > 1);
    table->setItem(row, ColKeyword, name);

    QTableWidgetItem *count = new QTableWidgetItem;
    /* setData rather than setText, so the column sorts as a NUMBER: as text, 9 images
       sorts after 1,000 and the list the user most wants to see first is scattered. */
    count->setData(Qt::DisplayRole, k.count);
    count->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    count->setFlags(count->flags() & ~Qt::ItemIsEditable);
    table->setItem(row, ColImages, count);

    const int action = cands.isEmpty() ? Remove : Move;
    QTableWidgetItem *act = new QTableWidgetItem(kActionText[action]);
    act->setData(Qt::UserRole, action);
    table->setItem(row, ColAction, act);

    /*  DEEPEST FIRST, which is what candidatesFor sorted them into. Where a name is under
        both Canada|BC and Location|Canada|BC, the longer path is the more completely
        filed one, and it is also the one that survives when the shorter branch is itself
        eventually filed under Location. */
    QTableWidgetItem *tgt = new QTableWidgetItem(cands.value(0));
    tgt->setData(Qt::UserRole, cands);
    if (action != Move)
        tgt->setForeground(table->palette().brush(QPalette::Disabled, QPalette::Text));
    table->setItem(row, ColTarget, tgt);

    QString match;
    if (cands.isEmpty()) match = "no match";
    else if (cands.size() == 1) match = "1 branch";
    else match = QString("%1 branches -- review").arg(cands.size());
    QTableWidgetItem *m = new QTableWidgetItem(match);
    m->setFlags(m->flags() & ~Qt::ItemIsEditable);
    if (cands.size() > 1) {
        QFont f = m->font();
        f.setBold(true);
        m->setFont(f);
        /*  The candidates, one per line and in the combo's order, because the point of
            the mark is that the row has an alternative and the user should not have to
            open the combo to see what it is. WITH THEIR IMAGE COUNTS: the branch that
            already holds 257 photographs is almost always the one meant, and that is
            the fact that decides these rows. */
        QStringList tip;
        for (const QString &c : cands) {
            const int n = counts.value(keywordFold(c), 0);
            tip << QString("%1  (%2)").arg(c,
                    n == 1 ? QString("1 image") : QString("%1 images").arg(n));
        }
        m->setToolTip(tip.join('\n'));
    }
    table->setItem(row, ColMatch, m);
}

QStringList KeywordTidyDlg::candidatesFor(const QString &name) const
{
/*
    DEEPEST FIRST, WITH THE SELF-NESTED ONES LAST.

    Depth is the ranking because a longer path is the more completely filed one: where a
    name sits under both Canada|BC and Location|Canada|BC, the second is where it ends up
    anyway once the stray Canada branch is itself filed.

    THE EXCEPTION IS THE ARTIFACT BRANCH. "Build vocabulary from catalog" faithfully
    records every path any file ever carried, and a library tagged by two applications
    carries some nonsense: Fauna|Bird|Gyrefalcon|Bird, Category|Moire|Category -- a node
    repeating an ancestor's name, which is what happens when a flat keyword was written
    into a hierarchical field. Those are the DEEPEST matches for exactly the names most
    in need of filing ("Bird", "Category"), so ranking on depth alone would pre-select
    rubbish for the biggest rows in the table. A path whose leaf appears twice in it is
    therefore ranked last, still offered, never chosen for the user.
*/
    auto selfNested = [](const QString &p) {
        const QStringList nodes = keywordNodes(p);
        if (nodes.size() < 2) return false;
        const QString leaf = keywordFold(nodes.last());
        for (int i = 0; i < nodes.size() - 1; ++i)
            if (keywordFold(nodes.at(i)) == leaf) return true;
        return false;
    };

    QStringList out = byLeafFold.value(keywordFold(name));
    std::sort(out.begin(), out.end(), [&selfNested](const QString &a, const QString &b) {
        const bool na = selfNested(a), nb = selfNested(b);
        if (na != nb) return nb;                            // sane paths first
        const int da = a.count('|'), db = b.count('|');
        if (da != db) return da > db;                       // then deepest
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    return out;
}

void KeywordTidyDlg::applyFilter()
{
    const QString needle = filterEdit->text().trimmed();
    const int show = showCombo->currentData().toInt();

    for (int r = 0; r < table->rowCount(); ++r) {
        const QTableWidgetItem *name = table->item(r, ColKeyword);
        const QTableWidgetItem *act = table->item(r, ColAction);
        const QTableWidgetItem *tgt = table->item(r, ColTarget);
        if (!name || !act || !tgt) continue;

        bool ok = needle.isEmpty() || name->text().contains(needle, Qt::CaseInsensitive);
        if (ok) {
            const int n = name->data(CandidatesRole).toStringList().size();
            const int a = act->data(Qt::UserRole).toInt();
            switch (show) {
            case ShowUnique:     ok = (n == 1); break;
            case ShowReview:     ok = (n > 1); break;
            case ShowNoMatch:    ok = (n == 0); break;
            /*  WILL MOVE, not "is set to Move": a Move with no branch in Goes to writes
                nothing, and listing it here would show a row under a heading the plan
                does not agree with. Same rule as plan(). */
            case ShowWillMove:   ok = (a == Move && !tgt->text().isEmpty()); break;
            case ShowWillRemove: ok = (a == Remove); break;
            case ShowSkipped:    ok = (a == Skip
                                       || (a == Move && tgt->text().isEmpty())); break;
            default: break;
            }
        }
        table->setRowHidden(r, !ok);
    }

    updateScopeHint();
    updateSummary();
}

QList<int> KeywordTidyDlg::targetRows() const
{
/*
    See the header: the selection when there is one, everything shown otherwise. A
    selected row that the filter has hidden is NOT included -- a hidden row is not one the
    user can see they are acting on, and Qt keeps a selection alive across a filter
    change, so honouring it would let a button reach rows that scrolled out of the
    question three filters ago.
*/
    QList<int> rows;
    const QModelIndexList sel = table->selectionModel()->selectedRows();
    for (const QModelIndex &idx : sel)
        if (!table->isRowHidden(idx.row())) rows << idx.row();
    if (!rows.isEmpty()) return rows;

    for (int r = 0; r < table->rowCount(); ++r)
        if (!table->isRowHidden(r)) rows << r;
    return rows;
}

void KeywordTidyDlg::updateScopeHint()
{
    const QModelIndexList sel = table->selectionModel()->selectedRows();
    int selected = 0;
    for (const QModelIndex &idx : sel)
        if (!table->isRowHidden(idx.row())) ++selected;

    int shown = 0;
    for (int r = 0; r < table->rowCount(); ++r) if (!table->isRowHidden(r)) ++shown;

    scopeHint->setText(selected > 0
        ? QString("   act on the %1 selected row%2").arg(selected)
                                                    .arg(selected == 1 ? "" : "s")
        : QString("   act on all %1 row%2 shown").arg(shown).arg(shown == 1 ? "" : "s"));
}

void KeywordTidyDlg::setRows(const QList<int> &rows, int action)
{
    /*  SORTING OFF FOR THE LOOP. A QTableWidget with sorting enabled re-sorts on every
        item change, so a bulk edit that changed the sort column would move the rows out
        from under the row indexes this was handed. */
    const bool wasSorting = table->isSortingEnabled();
    table->setSortingEnabled(false);
    bulkEdit = true;

    for (int r : rows) {
        const QTableWidgetItem *name = table->item(r, ColKeyword);
        QTableWidgetItem *act = table->item(r, ColAction);
        if (!name || !act) continue;
        /*  MOVE IS NOT SET ON A ROW WITH NOWHERE TO GO. It would write nothing, and a row
            reading "Move to" with an empty branch is a promise the plan does not keep.
            Those rows keep whatever they had -- Remove by default -- and the Show list
            is how they are dealt with as a group. */
        if (action == Move && name->data(CandidatesRole).toStringList().isEmpty())
            continue;
        /* Through setData so the itemChanged handler repaints the target column exactly
           as an edit by hand would. */
        act->setData(Qt::UserRole, action);
    }

    bulkEdit = false;
    table->setSortingEnabled(wasSorting);

    /*  The Show list can be showing a class of row that this has just changed ("Will be
        removed", and every row shown was set to Move), so the list is re-run rather than
        left describing a plan that has moved on. */
    applyFilter();
}

void KeywordTidyDlg::updateSummary()
{
    int move = 0, remove = 0, review = 0, images = 0;
    for (int r = 0; r < table->rowCount(); ++r) {
        const QTableWidgetItem *act = table->item(r, ColAction);
        const QTableWidgetItem *name = table->item(r, ColKeyword);
        const QTableWidgetItem *tgt = table->item(r, ColTarget);
        const QTableWidgetItem *cnt = table->item(r, ColImages);
        if (!act || !name || !tgt || !cnt) continue;
        const int a = act->data(Qt::UserRole).toInt();
        if (a == Move && !tgt->text().isEmpty()) { ++move; images += cnt->data(
                                                       Qt::DisplayRole).toInt(); }
        else if (a == Remove) { ++remove; images += cnt->data(Qt::DisplayRole).toInt(); }
        if (a != Skip && name->data(ReviewRole).toBool()) ++review;
    }

    /*  IMAGES ARE COUNTED PER KEYWORD, so "on N images" is an upper bound rather than a
        count of files: one image carrying six flat keywords is counted six times and
        written once. Said plainly here rather than reported as a number that will not
        match the one the operation ends with. */
    QString s = QString("Apply will move %1 and remove %2, leaving %3 alone -- up to %4 "
                        "keyword changes across the catalog.")
                    .arg(move).arg(remove).arg(table->rowCount() - move - remove)
                    .arg(QLocale().toString(images));
    if (review > 0)
        s += QString("  %1 of the rows it acts on matched more than one branch.")
                 .arg(review);
    summary->setText(s);

    /*  THE COUNT IS ON THE BUTTON, because that is the last thing read before a few
        thousand files are rewritten, and "Apply" alone says nothing about how much of the
        table it is about to carry out. */
    applyBtn->setText(move + remove > 0
                          ? QString("Apply to %1 keyword%2").arg(move + remove)
                                                            .arg(move + remove == 1
                                                                     ? "" : "s")
                          : QString("Apply"));
    applyBtn->setEnabled(move + remove > 0);
}

QList<KeywordTidyAction> KeywordTidyDlg::plan() const
{
    QList<KeywordTidyAction> out;
    for (int r = 0; r < table->rowCount(); ++r) {
        const QTableWidgetItem *name = table->item(r, ColKeyword);
        const QTableWidgetItem *act = table->item(r, ColAction);
        const QTableWidgetItem *tgt = table->item(r, ColTarget);
        if (!name || !act || !tgt) continue;
        const int a = act->data(Qt::UserRole).toInt();
        if (a == Skip) continue;

        KeywordTidyAction ta;
        ta.flat = name->text();
        if (a == Remove) {
            ta.remove = true;
        }
        else {
            /*  A Move with an empty or unchanged target is not a move. Normalised
                through the node splitter so a hand-typed " A | B " reaches the write
                path as the same identity the index holds. */
            const QString target = keywordNodes(tgt->text()).join('|');
            if (target.isEmpty()) continue;
            if (keywordFold(target) == keywordFold(ta.flat)) continue;
            ta.target = target;
        }
        out << ta;
    }
    return out;
}
