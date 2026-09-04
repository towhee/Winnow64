#include "Dialogs/catalogrootsdlg.h"
#include "Main/global.h"

#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>

CatalogRootsDlg::CatalogRootsDlg(QWidget *parent)
    : QDialog(parent)
{
    if (G::isLogger) G::log("CatalogRootsDlg::CatalogRootsDlg");

    setWindowTitle("Catalogued Folders");
    /* A tool window, not an application-modal dialog: a scan runs for minutes and this is
       where its state is shown. See the header. */
    setWindowFlag(Qt::Tool);
    setMinimumWidth(460);

    QLabel *intro = new QLabel(
        "Folders listed here are scanned in the background so their images can be "
        "found by the Catalog panel before you open them.\n\n"
        "Folders you browse are catalogued automatically -- this is only needed to "
        "index a library you have not visited yet.");
    intro->setWordWrap(true);

    QLabel *includeLabel = new QLabel("Catalogued folders");

    rootList = new QListWidget;
    rootList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    rootList->setMinimumHeight(140);

    recurseBox = new QCheckBox("Include subfolders");
    recurseBox->setChecked(true);

    addRootBtn = new QPushButton("Add...");
    removeRootBtn = new QPushButton("Remove");

    excludeLabel = new QLabel("Excluded folders");
    excludeList = new QListWidget;
    excludeList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    excludeList->setMinimumHeight(80);
    addExcludeBtn = new QPushButton("Add...");
    removeExcludeBtn = new QPushButton("Remove");
    removeExcludeBtn->setEnabled(false);
    excludeNote = new QLabel;
    excludeNote->setWordWrap(true);
    scanBtn = new QPushButton("Scan Now");
    closeBtn = new QPushButton("Close");
    removeRootBtn->setEnabled(false);

    statusLabel = new QLabel;
    statusLabel->setWordWrap(true);

    QHBoxLayout *rootBtns = new QHBoxLayout;
    rootBtns->setContentsMargins(0, 0, 0, 0);
    rootBtns->addWidget(addRootBtn);
    rootBtns->addWidget(removeRootBtn);
    rootBtns->addStretch(1);

    QHBoxLayout *excludeBtns = new QHBoxLayout;
    excludeBtns->setContentsMargins(0, 0, 0, 0);
    excludeBtns->addWidget(addExcludeBtn);
    excludeBtns->addWidget(removeExcludeBtn);
    excludeBtns->addStretch(1);

    QHBoxLayout *btns = new QHBoxLayout;
    btns->setContentsMargins(0, 0, 0, 0);
    btns->addStretch(1);
    btns->addWidget(scanBtn);
    btns->addWidget(closeBtn);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(8);
    layout->addWidget(intro);
    layout->addWidget(includeLabel);
    layout->addWidget(rootList, 2);
    layout->addWidget(recurseBox);
    layout->addLayout(rootBtns);
    layout->addWidget(excludeLabel);
    layout->addWidget(excludeList, 1);
    layout->addWidget(excludeNote);
    layout->addLayout(excludeBtns);
    layout->addWidget(statusLabel);
    layout->addLayout(btns);

    connect(rootList, &QListWidget::itemSelectionChanged, this, [this]{
        removeRootBtn->setEnabled(!scanning && !rootList->selectedItems().isEmpty());
    });

    connect(addRootBtn, &QPushButton::clicked, this, [this]{
        const QString dir = QFileDialog::getExistingDirectory(
            this, "Choose a folder to catalogue");
        if (dir.isEmpty()) return;
        /* Adding a folder already covered would scan it twice. An exact duplicate is
           caught here; an overlapping subtree is left alone, because the scanner
           de-duplicates the expanded folder list anyway. */
        for (int i = 0; i < rootList->count(); ++i)
            if (rootList->item(i)->text() == dir) return;
        rootList->addItem(dir);
        syncExcludeState();
        emit rootsChanged(roots(), recurseBox->isChecked(), excludes());
    });

    connect(removeRootBtn, &QPushButton::clicked, this, [this]{
        const auto chosen = rootList->selectedItems();
        for (QListWidgetItem *item : chosen)
            delete rootList->takeItem(rootList->row(item));
        syncExcludeState();
        emit rootsChanged(roots(), recurseBox->isChecked(), excludes());
        /* Removing a root does NOT un-catalogue what it contributed. Those images are
           still real and still findable, and silently dropping thousands of rows because
           a folder was taken off the scan list would be a surprising amount of deletion
           for what reads as "stop scanning this". */
    });

    connect(recurseBox, &QCheckBox::toggled, this, [this](bool on){
        /* Without recursion a root is one folder deep and there is no subtree left to
           carve anything out of, so the exclusions go grey with the reason in place
           rather than sitting there looking like they still apply. */
        syncExcludeState();
        emit rootsChanged(roots(), on, excludes());
    });

    connect(excludeList, &QListWidget::itemSelectionChanged, this, [this]{
        removeExcludeBtn->setEnabled(!scanning && !excludeList->selectedItems().isEmpty());
    });

    connect(addExcludeBtn, &QPushButton::clicked, this, [this]{
        /* Start the browser inside the first catalogued folder: an exclusion is only
           ever chosen from within one, so that is where the user is heading. */
        const QString start = rootList->count() ? rootList->item(0)->text() : QString();
        const QString dir = QFileDialog::getExistingDirectory(
            this, "Choose a folder to exclude from the catalog", start);
        if (dir.isEmpty()) return;
        for (int i = 0; i < excludeList->count(); ++i)
            if (excludeList->item(i)->text() == dir) return;
        /* An exclusion outside every catalogued tree excludes nothing, and silently
           accepting it would leave the user believing a folder was carved out when it
           was never going to be scanned in the first place. Say so, in place. */
        bool inside = false;
        for (int i = 0; i < rootList->count(); ++i) {
            const QString root = rootList->item(i)->text();
            if (dir == root || dir.startsWith(root + "/")) { inside = true; break; }
        }
        if (!inside) {
            excludeNote->setText("\"" + QDir(dir).dirName() + "\" is not inside any "
                                 "catalogued folder, so it is not being scanned anyway.");
            return;
        }
        excludeList->addItem(dir);
        syncExcludeState();
        emit rootsChanged(roots(), recurseBox->isChecked(), excludes());
    });

    connect(removeExcludeBtn, &QPushButton::clicked, this, [this]{
        const auto chosen = excludeList->selectedItems();
        for (QListWidgetItem *item : chosen)
            delete excludeList->takeItem(excludeList->row(item));
        syncExcludeState();
        emit rootsChanged(roots(), recurseBox->isChecked(), excludes());
        /* Un-excluding a folder does not index it either: it is picked up by the next
           scan, the same way any other folder is. */
    });

    connect(scanBtn, &QPushButton::clicked, this, [this]{
        if (scanning) emit stopScanRequested();
        else emit scanRequested();
    });

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
}

void CatalogRootsDlg::setRoots(const QStringList &r, bool recurse,
                               const QStringList &ex)
{
    rootList->clear();
    rootList->addItems(r);
    excludeList->clear();
    excludeList->addItems(ex);
    QSignalBlocker block(recurseBox);       // this is a restore, not a user edit
    recurseBox->setChecked(recurse);
    removeRootBtn->setEnabled(false);
    syncExcludeState();
}

QStringList CatalogRootsDlg::excludes() const
{
    QStringList out;
    for (int i = 0; i < excludeList->count(); ++i) out << excludeList->item(i)->text();
    return out;
}

void CatalogRootsDlg::syncExcludeState()
{
/*
    An exclusion only means something when there is a subtree for it to carve out of, so
    when there is no catalogued folder or subfolders are switched off the whole exclusion
    group goes grey WITH THE REASON NEXT TO IT. A control that is simply enabled and does
    nothing, or one that explains itself in a popup after the fact, both leave the user to
    work out why their choice had no effect.
*/
    const bool usable = !scanning && rootList->count() > 0 && recurseBox->isChecked();
    excludeLabel->setEnabled(usable);
    excludeList->setEnabled(usable);
    addExcludeBtn->setEnabled(usable);
    removeExcludeBtn->setEnabled(usable && !excludeList->selectedItems().isEmpty());

    if (scanning)
        excludeNote->setText("Cannot be changed while a scan is running.");
    else if (rootList->count() == 0)
        excludeNote->setText("Add a catalogued folder above before excluding anything.");
    else if (!recurseBox->isChecked())
        excludeNote->setText("Only applies when \"Include subfolders\" is ticked.");
    else
        excludeNote->setText("An excluded folder and everything inside it is skipped.");
}

QStringList CatalogRootsDlg::roots() const
{
    QStringList out;
    for (int i = 0; i < rootList->count(); ++i) out << rootList->item(i)->text();
    return out;
}

bool CatalogRootsDlg::rootsRecurse() const
{
    return recurseBox->isChecked();
}

void CatalogRootsDlg::setScanning(bool on)
{
/*
    While a scan runs the button becomes Stop and the list is frozen: editing the roots
    underneath a running scan would leave the user unsure whether the folder they just
    added is being scanned or not.
*/
    scanning = on;
    scanBtn->setText(on ? "Stop" : "Scan Now");
    rootList->setEnabled(!on);
    recurseBox->setEnabled(!on);
    addRootBtn->setEnabled(!on);
    removeRootBtn->setEnabled(!on && !rootList->selectedItems().isEmpty());
    syncExcludeState();
}

void CatalogRootsDlg::setCatalogStatus(const QString &text)
{
    statusLabel->setText(text);
}
