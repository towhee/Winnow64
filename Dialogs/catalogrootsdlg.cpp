#include "Dialogs/catalogrootsdlg.h"
#include "Main/global.h"

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

    rootList = new QListWidget;
    rootList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    rootList->setMinimumHeight(140);

    recurseBox = new QCheckBox("Include subfolders");
    recurseBox->setChecked(true);

    addRootBtn = new QPushButton("Add...");
    removeRootBtn = new QPushButton("Remove");
    scanBtn = new QPushButton("Scan Now");
    closeBtn = new QPushButton("Close");
    removeRootBtn->setEnabled(false);

    statusLabel = new QLabel;
    statusLabel->setWordWrap(true);

    QHBoxLayout *btns = new QHBoxLayout;
    btns->setContentsMargins(0, 0, 0, 0);
    btns->addWidget(addRootBtn);
    btns->addWidget(removeRootBtn);
    btns->addStretch(1);
    btns->addWidget(scanBtn);
    btns->addWidget(closeBtn);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(8);
    layout->addWidget(intro);
    layout->addWidget(rootList, 1);
    layout->addWidget(recurseBox);
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
        emit rootsChanged(roots(), recurseBox->isChecked());
    });

    connect(removeRootBtn, &QPushButton::clicked, this, [this]{
        const auto chosen = rootList->selectedItems();
        for (QListWidgetItem *item : chosen)
            delete rootList->takeItem(rootList->row(item));
        emit rootsChanged(roots(), recurseBox->isChecked());
        /* Removing a root does NOT un-catalogue what it contributed. Those images are
           still real and still findable, and silently dropping thousands of rows because
           a folder was taken off the scan list would be a surprising amount of deletion
           for what reads as "stop scanning this". */
    });

    connect(recurseBox, &QCheckBox::toggled, this, [this](bool on){
        emit rootsChanged(roots(), on);
    });

    connect(scanBtn, &QPushButton::clicked, this, [this]{
        if (scanning) emit stopScanRequested();
        else emit scanRequested();
    });

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
}

void CatalogRootsDlg::setRoots(const QStringList &r, bool recurse)
{
    rootList->clear();
    rootList->addItems(r);
    QSignalBlocker block(recurseBox);       // this is a restore, not a user edit
    recurseBox->setChecked(recurse);
    removeRootBtn->setEnabled(false);
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
}

void CatalogRootsDlg::setCatalogStatus(const QString &text)
{
    statusLabel->setText(text);
}
