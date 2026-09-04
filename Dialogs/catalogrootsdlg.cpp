#include "Dialogs/catalogrootsdlg.h"
#include "Main/global.h"

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QFontMetrics>
#include <QHeaderView>
#include <QLocale>
#include <QVBoxLayout>

namespace {

/* Column order is the order the rule reads in: what this row does, to which folder, how
   far down. */
enum Column { ColMode = 0, ColPath = 1, ColRecurse = 2, ColImages = 3, ColCount = 4 };

}  // namespace

CatalogRootsDlg::CatalogRootsDlg(QWidget *parent)
    : QDialog(parent)
{
    if (G::isLogger) G::log("CatalogRootsDlg::CatalogRootsDlg");

    setWindowTitle("Manage Catalog");
    /* A tool window, not an application-modal dialog: a scan runs for minutes and this is
       where its state is shown. See the header. */
    setWindowFlag(Qt::Tool);
    setMinimumWidth(672);

    QLabel *intro = new QLabel(
        "Folders included here are scanned in the background so their images can be "
        "found by the Catalog panel before you open them.\n\n"
        "Folders you browse are catalogued automatically -- this is only needed to "
        "index a library you have not visited yet. Exclude a folder to carve a branch "
        "out of an included folder; an exclusion always wins, wherever it sits in the "
        "table.");
    intro->setWordWrap(true);

    table = new QTableWidget(0, ColCount);
    table->setHorizontalHeaderLabels(
        QStringList() << "Action" << "Folder path" << "Include Subfolders"
                      << "Images");
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode(ColMode, QHeaderView::ResizeToContents);
    /* The path is the column that varies in length and the one worth reading, so it takes
       whatever the other two do not. */
    table->horizontalHeader()->setSectionResizeMode(ColPath, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(ColRecurse, QHeaderView::ResizeToContents);
    /* Sized once for the widest number it will ever hold rather than to its contents:
       ResizeToContents makes the column jump every time a count lands, which drags the
       path column with it and makes a settled table look like it is still working. */
    {
        QFontMetrics fm(table->font());
        const int wide = fm.horizontalAdvance(QLocale().toString(9999999)) + 28;
        const int header = fm.horizontalAdvance("Images") + 28;
        table->horizontalHeader()->setSectionResizeMode(ColImages, QHeaderView::Fixed);
        table->setColumnWidth(ColImages, qMax(wide, header));
    }
    table->setMinimumHeight(160);

    noteLabel = new QLabel;
    noteLabel->setWordWrap(true);
    statusLabel = new QLabel;
    statusLabel->setWordWrap(true);
    /* MW composes this one, and colours the clause that says a scan is owed. */
    statusLabel->setTextFormat(Qt::RichText);

    addBtn = new QPushButton("Append");
    removeBtn = new QPushButton("Remove");
    scanBtn = new QPushButton("Scan");
    closeBtn = new QPushButton("Close");
    removeBtn->setEnabled(false);

    QHBoxLayout *btns = new QHBoxLayout;
    btns->setContentsMargins(0, 0, 0, 0);
    btns->addWidget(addBtn);
    btns->addWidget(removeBtn);
    btns->addStretch(1);
    btns->addWidget(scanBtn);
    btns->addWidget(closeBtn);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(8);
    layout->addWidget(intro);
    /* The table is a different kind of thing from the paragraph above it and the buttons
       below it, and a blank line either side is what says so. */
    layout->addSpacing(8);
    layout->addWidget(table, 1);
    layout->addWidget(noteLabel);
    layout->addWidget(statusLabel);
    layout->addSpacing(8);
    layout->addLayout(btns);

    /* The only editable item state is the Include Subfolders check, so every itemChanged
       is a user toggling one -- the guard is populating, not the column. */
    connect(table, &QTableWidget::itemChanged, this, [this]{ noteEdit(); });

    connect(table, &QTableWidget::itemSelectionChanged, this, [this]{
        removeBtn->setEnabled(!scanning && !table->selectedItems().isEmpty());
    });

    connect(addBtn, &QPushButton::clicked, this, [this]{
        /* Start the browser at the first included folder: a new row is nearly always
           either a sibling of it or a branch inside it. */
        QString start;
        for (int r = 0; r < table->rowCount() && start.isEmpty(); ++r) {
            auto *mode = qobject_cast<QComboBox*>(table->cellWidget(r, ColMode));
            if (mode && mode->currentIndex() == 0)
                start = table->item(r, ColPath)->text();
        }
        const QString dir = QFileDialog::getExistingDirectory(
            this, "Choose a folder to include in or exclude from the catalog", start);
        if (dir.isEmpty()) return;
        /* The same folder twice would be two rules about one folder, which is either a
           duplicate or a contradiction; neither is worth a table row. */
        for (int r = 0; r < table->rowCount(); ++r)
            if (table->item(r, ColPath)->text() == catalogScopeNormalize(dir)) {
                table->selectRow(r);
                noteLabel->setText("\"" + QDir(dir).dirName() + "\" is already in the "
                                   "table -- change that row instead.");
                return;
            }
        CatalogScopeEntry e;
        e.path = catalogScopeNormalize(dir);
        addRow(e);
        table->selectRow(table->rowCount() - 1);
        noteEdit();
    });

    connect(removeBtn, &QPushButton::clicked, this, [this]{
        /* Rows removed from the bottom up, so each index is still valid when it is
           reached. */
        QList<int> rows;
        const auto ranges = table->selectedRanges();
        for (const QTableWidgetSelectionRange &range : ranges)
            for (int r = range.topRow(); r <= range.bottomRow(); ++r) rows << r;
        std::sort(rows.begin(), rows.end(), std::greater<int>());
        for (int r : rows) table->removeRow(r);
        /*  REMOVING A ROW UN-CATALOGUES WHAT IT CONTRIBUTED, and this is the ordinary
            scopeChanged that says so: the table states what the catalog HOLDS, not
            merely what a scan visits, so no separate signal or confirmation belongs
            here. MW counts what the removal disowns, asks, and puts the rows back if the
            user declines -- which is why this does not need to know whether the deletion
            was accepted. Removing an exclusion still indexes nothing by itself: the next
            scan picks that folder up like any other. */
        noteEdit();
    });

    connect(scanBtn, &QPushButton::clicked, this, [this]{
        if (scanning) emit stopScanRequested();
        else emit scanRequested();
    });

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);

    updateNote();
}

void CatalogRootsDlg::addRow(const CatalogScopeEntry &e)
{
    const int r = table->rowCount();
    table->insertRow(r);

    QComboBox *mode = new QComboBox;
    mode->addItem("Include");
    mode->addItem("Exclude");
    mode->setCurrentIndex(e.include ? 0 : 1);
    table->setCellWidget(r, ColMode, mode);
    connect(mode, &QComboBox::currentIndexChanged, this, [this]{ noteEdit(); });

    /* Normalised on the way in, not on the way out: every rule here is a prefix test,
       and one trailing slash makes them all quietly false (see catalogScopeNormalize). */
    const QString folder = catalogScopeNormalize(e.path);
    QTableWidgetItem *path = new QTableWidgetItem(folder);
    path->setToolTip(folder);
    table->setItem(r, ColPath, path);

    /* A checkable item rather than a QCheckBox widget: the row must stay selectable by
       clicking anywhere across it, and a widget in the cell swallows that click. */
    QTableWidgetItem *rec = new QTableWidgetItem;
    rec->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    rec->setCheckState(e.recurse ? Qt::Checked : Qt::Unchecked);
    rec->setTextAlignment(Qt::AlignCenter);
    rec->setToolTip("On: this row applies to the folder and everything under it.\n"
                    "Off: it applies to this folder alone.");
    table->setItem(r, ColRecurse, rec);

    QTableWidgetItem *images = new QTableWidgetItem(QString(QChar(0x2014)));
    images->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    images->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    images->setToolTip("Images this folder holds ON DISK, counted now -- not what the "
                       "catalog has indexed.\n"
                       "The includes less the excludes is what the catalog should hold "
                       "once a Scan has run.");
    table->setItem(r, ColImages, images);
}

void CatalogRootsDlg::setRowCounts(const QVector<int> &counts)
{
/*
    Written with populating held, because these are items in the same table whose
    itemChanged is how a user edit is noticed -- without it, showing a count would look
    like an edit, which would ask MW for the counts again, forever.
*/
    const bool wasPopulating = populating;
    populating = true;
    for (int r = 0; r < table->rowCount(); ++r) {
        QTableWidgetItem *item = table->item(r, ColImages);
        if (!item) continue;
        const int n = r < counts.size() ? counts.at(r) : -1;
        /* -2 counting, -1 unknown (the folder is not there, or unmounted), 0 or more the
           answer. A stale number left in place would be read as a fresh one. */
        if (n == -2) item->setText("...");
        else if (n < 0) item->setText(QString(QChar(0x2014)));
        else item->setText(QLocale().toString(n));
    }
    populating = wasPopulating;
}

void CatalogRootsDlg::setScope(const CatalogScope &scope)
{
    populating = true;                  // a restore is not a user edit
    table->setRowCount(0);
    for (const CatalogScopeEntry &e : scope) addRow(e);
    populating = false;
    removeBtn->setEnabled(false);
    updateNote();
}

CatalogScope CatalogRootsDlg::scope() const
{
    CatalogScope out;
    for (int r = 0; r < table->rowCount(); ++r) {
        const QTableWidgetItem *path = table->item(r, ColPath);
        const QTableWidgetItem *rec = table->item(r, ColRecurse);
        auto *mode = qobject_cast<QComboBox*>(table->cellWidget(r, ColMode));
        if (!path || !rec || !mode) continue;
        CatalogScopeEntry e;
        e.path = path->text();
        e.include = mode->currentIndex() == 0;
        e.recurse = rec->checkState() == Qt::Checked;
        out << e;
    }
    return out;
}

void CatalogRootsDlg::noteEdit()
{
    if (populating) return;
    updateNote();
    emit scopeChanged(scope());
}

void CatalogRootsDlg::updateNote()
{
/*
    The note carries whatever the table cannot say for itself, and it says it HERE rather
    than in a popup after the fact: a rule that quietly achieves nothing is exactly the
    kind of thing a user only discovers hours later, when a search comes back short.
*/
    if (scanning) {
        noteLabel->setText("The table cannot be changed while a scan is running.");
        return;
    }

    const CatalogScope s = scope();
    bool anyInclude = false;
    for (const CatalogScopeEntry &e : s) if (e.include) anyInclude = true;
    if (!anyInclude) {
        noteLabel->setText(s.isEmpty()
            ? "Add a folder to have it indexed in the background."
            : "Nothing is included, so a scan would index nothing.");
        return;
    }

    /* An exclude only means something inside an included tree. */
    for (const CatalogScopeEntry &e : s) {
        if (e.include || e.path.isEmpty()) continue;
        if (!catalogScopeIncludes(s, e.path)) {
            noteLabel->setText("\"" + QDir(e.path).dirName() + "\" is not inside any "
                               "included folder, so excluding it changes nothing.");
            return;
        }
    }

    noteLabel->setText("These folders are the catalog: removing a row forgets what it "
                       "contributed. The image files are not affected.");
}

void CatalogRootsDlg::setScanning(bool on)
{
/*
    While a scan runs the button becomes Stop and the table is frozen: editing the scope
    underneath a running scan would leave the user unsure whether the folder they just
    added is being scanned or not.
*/
    scanning = on;
    scanBtn->setText(on ? "Stop" : "Scan");
    table->setEnabled(!on);
    addBtn->setEnabled(!on);
    removeBtn->setEnabled(!on && !table->selectedItems().isEmpty());
    updateNote();
}

void CatalogRootsDlg::setCatalogStatus(const QString &text)
{
    statusLabel->setText(text);
}
