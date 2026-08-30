#include "Views/catalogview.h"
#include "Main/global.h"

#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QVBoxLayout>

namespace {

/* The tree stores the keyword's full hierarchical path here; the visible column shows
   only the leaf, so a deep hierarchy stays readable in a narrow dock. */
constexpr int kPathRole = Qt::UserRole + 1;

/* Keystrokes are coalesced into one query. Long enough that ordinary typing produces a
   single search, short enough to feel immediate when the user stops. */
constexpr int kDebounceMs = 250;

}  // namespace

CatalogView::CatalogView(QWidget *parent)
    : QWidget(parent)
{
    if (G::isLogger) G::log("CatalogView::CatalogView");

    searchEdit = new QLineEdit;
    searchEdit->setPlaceholderText("Search keywords, title, camera...");
    searchEdit->setClearButtonEnabled(true);

    minRating = new QSpinBox;
    minRating->setRange(0, 5);
    minRating->setPrefix("★ ≥ ");
    minRating->setSpecialValueText("Any rating");
    minRating->setToolTip("Only show images rated at least this many stars.");

    keywordTree = new QTreeWidget;
    keywordTree->setHeaderHidden(true);
    keywordTree->setColumnCount(1);
    keywordTree->setUniformRowHeights(true);
    keywordTree->setToolTip("Click a keyword to search for it. Click it again to clear.\n"
                            "A parent keyword also finds images tagged only with its "
                            "children.");

    resultLabel = new QLabel;
    resultLabel->setWordWrap(true);

    /* The panel's own explanation of why it cannot be used, shown in place of a popup
       fired after the user has already typed something. */
    unavailableLabel = new QLabel;
    unavailableLabel->setWordWrap(true);
    unavailableLabel->setVisible(false);

    loadBtn = new QPushButton("Load Results");
    loadBtn->setEnabled(false);
    loadBtn->setToolTip("Load the matching images into the grid, replacing what is "
                        "loaded now.");
    /* min-width: 0 defeats the global "QPushButton { min-width: 100px }" (widgetcss),
       which otherwise becomes a hard floor on how narrow this dock can be made. */
    loadBtn->setStyleSheet("QPushButton { min-width: 0; }");

    QHBoxLayout *topRow = new QHBoxLayout;
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->addWidget(searchEdit, 1);
    topRow->addWidget(minRating);

    /* The designated roots -- folders scanned in the background so search covers a
       library the user has not browsed yet. Grouped and placed below the search so the
       panel reads top-down as "find things" then "what is indexed". */
    rootList = new QListWidget;
    rootList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    rootList->setToolTip("Folders scanned in the background so their images are "
                         "searchable before you open them.");
    rootList->setMaximumHeight(90);

    recurseBox = new QCheckBox("Include subfolders");
    recurseBox->setChecked(true);

    addRootBtn = new QPushButton("Add...");
    removeRootBtn = new QPushButton("Remove");
    scanBtn = new QPushButton("Scan Now");
    for (QPushButton *b : {addRootBtn, removeRootBtn, scanBtn})
        b->setStyleSheet("QPushButton { min-width: 0; }");
    removeRootBtn->setEnabled(false);

    QHBoxLayout *rootBtns = new QHBoxLayout;
    rootBtns->setContentsMargins(0, 0, 0, 0);
    rootBtns->addWidget(addRootBtn);
    rootBtns->addWidget(removeRootBtn);
    rootBtns->addStretch(1);
    rootBtns->addWidget(scanBtn);

    QVBoxLayout *rootsLayout = new QVBoxLayout;
    rootsLayout->setContentsMargins(4, 4, 4, 4);
    rootsLayout->setSpacing(4);
    rootsLayout->addWidget(rootList);
    rootsLayout->addWidget(recurseBox);
    rootsLayout->addLayout(rootBtns);

    QGroupBox *rootsBox = new QGroupBox("Catalogued folders");
    rootsBox->setLayout(rootsLayout);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addWidget(unavailableLabel);
    layout->addLayout(topRow);
    layout->addWidget(keywordTree, 1);
    layout->addWidget(resultLabel);
    layout->addWidget(loadBtn);
    layout->addWidget(rootsBox);

    debounce = new QTimer(this);
    debounce->setSingleShot(true);
    debounce->setInterval(kDebounceMs);
    connect(debounce, &QTimer::timeout, this, &CatalogView::runSearch);

    connect(searchEdit, &QLineEdit::textChanged, this, [this]{ debounce->start(); });
    /* Enter runs it now rather than waiting out the debounce. */
    connect(searchEdit, &QLineEdit::returnPressed, this, &CatalogView::runSearch);
    connect(minRating, &QSpinBox::valueChanged, this, [this]{ debounce->start(); });

    connect(keywordTree, &QTreeWidget::itemClicked, this,
            [this](QTreeWidgetItem *item, int) {
                const QString path = item->data(0, kPathRole).toString();
                /* Clicking the selected keyword again clears it -- otherwise the only way
                   out of a keyword search is to know that ctrl-click deselects. */
                selectedKeyword = (path == selectedKeyword) ? QString() : path;
                if (selectedKeyword.isEmpty()) keywordTree->clearSelection();
                runSearch();
            });

    connect(loadBtn, &QPushButton::clicked, this, [this]{
        if (!results.isEmpty()) emit loadResults(results);
    });

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

    setAvailability();
}

void CatalogView::setAvailability()
{
/*
    Two reasons the panel cannot do anything: the index will not open at all, or it is
    open but empty. Both disable the controls and say so HERE, in the panel. The
    alternative -- letting the user type a search and only then explaining -- is the
    pattern this deliberately avoids.
*/
    const bool open = Catalog::instance().isAvailable();
    const int n = open ? Catalog::instance().count() : 0;
    const bool usable = open && n > 0;

    /* Only the SEARCH half is disabled. The roots controls below stay live even with an
       empty catalog -- they are how the user fills it, and disabling them would leave no
       way out of "nothing is catalogued yet". */
    searchEdit->setEnabled(usable);
    minRating->setEnabled(usable);
    keywordTree->setEnabled(usable);

    if (!open) {
        unavailableLabel->setText(
            "The catalog is unavailable -- the local index database could not be "
            "opened. Browsing is unaffected; searching is not available until it can "
            "be rebuilt.");
        /* Nothing can be indexed either, so scanning is pointless here. */
        addRootBtn->setEnabled(false);
        scanBtn->setEnabled(false);
    }
    else if (n == 0) {
        unavailableLabel->setText(
            "Nothing is catalogued yet. Folders are catalogued as you open them, or "
            "add a folder below and press Scan Now to index it in the background.");
        addRootBtn->setEnabled(!scanning);
        scanBtn->setEnabled(true);
    }
    else {
        addRootBtn->setEnabled(!scanning);
        scanBtn->setEnabled(true);
    }
    unavailableLabel->setVisible(!usable);

    if (!usable) {
        resultLabel->clear();
        loadBtn->setEnabled(false);
    }
}

void CatalogView::focusSearch()
{
    searchEdit->setFocus();
    searchEdit->selectAll();
}

void CatalogView::refresh()
{
    if (G::isLogger) G::log("CatalogView::refresh");
    setAvailability();
    rebuildKeywordTree();
    /* Re-run whatever is in the box, so the counts reflect a catalog that just grew. */
    if (searchEdit->isEnabled()) runSearch();
}

void CatalogView::rebuildKeywordTree()
{
/*
    Render the keyword vocabulary as a tree.

    The catalog stores one row per hierarchy NODE with its full path, so the tree is built
    by walking each path and creating any missing ancestor as it goes -- the rows arrive
    ordered by path, which means a parent is always seen before its children. Flat
    keywords (path empty) have no ancestors and sit at the top level.

    Counts come from the catalog, not from summing children: an ancestor row is linked to
    every image beneath it, so its count is already the total.
*/
    const QString wasSelected = selectedKeyword;
    keywordTree->clear();
    if (!Catalog::instance().isAvailable()) return;

    QHash<QString, QTreeWidgetItem *> byPath;
    QTreeWidgetItem *toSelect = nullptr;

    for (const CatalogKeyword &k : Catalog::instance().keywords()) {
        const QString label = QString("%1  (%2)").arg(k.name).arg(k.count);

        if (k.path.isEmpty()) {
            /* A flat dc:subject keyword. Keyed by name so it cannot collide with a
               hierarchy node of the same name, which is a different fact. */
            QTreeWidgetItem *item = new QTreeWidgetItem(keywordTree);
            item->setText(0, label);
            item->setData(0, kPathRole, k.name);
            if (k.name == wasSelected) toSelect = item;
            continue;
        }

        /* Find or create every ancestor, then this node. */
        QTreeWidgetItem *parent = nullptr;
        QString acc;
        const QStringList parts = k.path.split('|', Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            acc = acc.isEmpty() ? part : acc + '|' + part;
            QTreeWidgetItem *node = byPath.value(acc, nullptr);
            if (!node) {
                node = parent ? new QTreeWidgetItem(parent)
                              : new QTreeWidgetItem(keywordTree);
                node->setText(0, part);
                node->setData(0, kPathRole, acc);
                byPath.insert(acc, node);
            }
            parent = node;
        }
        if (parent) {
            parent->setText(0, label);
            if (k.path == wasSelected) toSelect = parent;
        }
    }

    keywordTree->sortItems(0, Qt::AscendingOrder);
    if (toSelect) {
        toSelect->setSelected(true);
        keywordTree->scrollToItem(toSelect);
    }
    else {
        /* The keyword is gone from the catalog (its last image was removed), so the
           restriction it described no longer means anything. */
        selectedKeyword.clear();
    }
}

CatalogQuery CatalogView::currentQuery() const
{
    CatalogQuery q;
    q.text = searchEdit->text();
    q.keyword = selectedKeyword;
    q.minRating = minRating->value();
    return q;
}

void CatalogView::runSearch()
{
    if (G::isLogger) G::log("CatalogView::runSearch");
    debounce->stop();

    if (!searchEdit->isEnabled()) return;

    const CatalogQuery q = currentQuery();
    /* An empty query would match the whole catalog. Reporting "247,000 images" and
       offering to load them is not a useful answer to having typed nothing, so the panel
       stays quiet until the user has actually asked for something. */
    if (q.text.trimmed().isEmpty() && q.keyword.isEmpty() && q.minRating == 0) {
        results.clear();
        totalMatches = 0;
        resultLabel->setText(QString("%1 images catalogued.")
                                 .arg(Catalog::instance().count()));
        loadBtn->setEnabled(false);
        return;
    }

    results = Catalog::instance().search(q, kResultLimit, &totalMatches);

    if (totalMatches == 0) {
        resultLabel->setText("No matches.");
    }
    else if (results.size() < totalMatches) {
        /* Say plainly that this is a subset, and what would be loaded. */
        resultLabel->setText(QString("%1 matches -- the first %2 will be loaded.")
                                 .arg(totalMatches).arg(results.size()));
    }
    else {
        resultLabel->setText(QString("%1 %2 found.")
                                 .arg(totalMatches)
                                 .arg(totalMatches == 1 ? "image" : "images"));
    }
    loadBtn->setEnabled(!results.isEmpty());
}

void CatalogView::setRoots(const QStringList &r, bool recurse)
{
    rootList->clear();
    rootList->addItems(r);
    QSignalBlocker block(recurseBox);       // this is a restore, not a user edit
    recurseBox->setChecked(recurse);
    removeRootBtn->setEnabled(false);
    setAvailability();
}

QStringList CatalogView::roots() const
{
    QStringList out;
    for (int i = 0; i < rootList->count(); ++i) out << rootList->item(i)->text();
    return out;
}

bool CatalogView::rootsRecurse() const
{
    return recurseBox->isChecked();
}

void CatalogView::setScanning(bool on)
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
