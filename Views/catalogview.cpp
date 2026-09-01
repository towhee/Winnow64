#include "Views/catalogview.h"
#include "Main/global.h"
#include "Metadata/keywordflatten.h"

#include <QApplication>
#include <QFileDialog>
#include <QFormLayout>
#include <QMenu>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QVBoxLayout>

namespace {

/* The keyword name, kept beside the display text because the visible label carries the
   image count too ("Heron  (128)") and the query needs the bare name. */
constexpr int kNameRole = Qt::UserRole + 1;

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
    /* Match the Filters panel: a facet list has no notion of a "current" row, and the
       selection highlight only competed with the include/exclude colouring that carries
       the actual meaning. */
    keywordTree->setSelectionMode(QAbstractItemView::NoSelection);
    keywordTree->setFocusPolicy(Qt::NoFocus);
    keywordTree->setToolTip("Click a keyword to include it, Opt+click to exclude it, or "
                            "right-click for both.\nClick again to clear.\n\n"
                            "Keywords are flat: a hierarchy contributes each of its "
                            "levels as its own keyword,\nso including a parent name "
                            "finds everything that was beneath it. A keyword used\nunder "
                            "more than one parent is highlighted -- exclude a parent to "
                            "tell them apart.");

    resultLabel = new QLabel;
    resultLabel->setWordWrap(true);

    /* The panel's own explanation of why it cannot be used, shown in place of a popup
       fired after the user has already typed something. */
    unavailableLabel = new QLabel;
    unavailableLabel->setWordWrap(true);
    unavailableLabel->setVisible(false);

    loadBtn = new QPushButton("Load");
    loadBtn->setEnabled(false);
    loadBtn->setToolTip("Load the matching images into the grid, replacing what is "
                        "loaded now.");
    /* min-width: 0 defeats the global "QPushButton { min-width: 100px }" (widgetcss),
       which otherwise becomes a hard floor on how narrow this dock can be made. */
    loadBtn->setStyleSheet("QPushButton { min-width: 0; }");

    /* Add rather than replace, so two searches can be compared side by side. Without it
       every search throws the previous one away, which makes "the herons AND the eagles"
       impossible to assemble however the query is written. */
    addBtn = new QPushButton("Add");
    addBtn->setEnabled(false);
    addBtn->setToolTip("Add the matching images to what is already loaded, instead of "
                       "replacing it.");
    addBtn->setStyleSheet("QPushButton { min-width: 0; }");

    QHBoxLayout *loadRow = new QHBoxLayout;
    loadRow->setContentsMargins(0, 0, 0, 0);
    loadRow->addWidget(loadBtn, 1);
    loadRow->addWidget(addBtn, 1);

    QHBoxLayout *topRow = new QHBoxLayout;
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->addWidget(searchEdit, 1);
    topRow->addWidget(minRating);

    /* What is indexed, and the way in to changing it. The editor is a separate dialog:
       the root list is configuration, revisited rarely, and it was taking the lower third
       of a panel used constantly. The status line is what stops that being a hidden
       feature -- it says what is catalogued, right where a search that found nothing
       would make the user wonder. */
    catalogStatusLabel = new QLabel;
    catalogStatusLabel->setWordWrap(true);

    manageRootsBtn = new QPushButton("Manage...");
    manageRootsBtn->setToolTip("Choose which folders are indexed in the background, and "
                               "scan them now.");
    manageRootsBtn->setStyleSheet("QPushButton { min-width: 0; }");

    QHBoxLayout *statusRow = new QHBoxLayout;
    statusRow->setContentsMargins(0, 0, 0, 0);
    statusRow->addWidget(catalogStatusLabel, 1);
    statusRow->addWidget(manageRootsBtn);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addWidget(unavailableLabel);
    layout->addLayout(topRow);
    layout->addWidget(keywordTree, 1);
    layout->addWidget(resultLabel);
    layout->addLayout(loadRow);
    layout->addLayout(statusRow);

    debounce = new QTimer(this);
    debounce->setSingleShot(true);
    debounce->setInterval(kDebounceMs);
    connect(debounce, &QTimer::timeout, this, &CatalogView::runSearch);

    connect(searchEdit, &QLineEdit::textChanged, this, [this]{ debounce->start(); });
    /* Enter runs it now rather than waiting out the debounce. */
    connect(searchEdit, &QLineEdit::returnPressed, this, &CatalogView::runSearch);
    connect(minRating, &QSpinBox::valueChanged, this, [this]{ debounce->start(); });

    /* Click includes, Opt+click excludes -- the same modifier idiom the mask combine
       tools use, so the gesture is one the user has already met. Clicking an item that is
       already in that state clears it, which is the way back out; without that the only
       exit from a keyword filter would be to know some other gesture. */
    /* Clicking the INDICATOR makes Qt toggle the box itself and emit itemChanged before
       itemClicked. Acting again in itemClicked would undo it -- the two cancel out and
       the checkbox appears not to respond at all -- so itemChanged records what Qt did
       and itemClicked only handles the cases Qt does not: a click on the text, and
       Opt+click for exclude. */
    connect(keywordTree, &QTreeWidget::itemChanged, this,
            [this](QTreeWidgetItem *item, int) {
                keywordCheckJustChanged = true;
                const QString name = item->data(0, kNameRole).toString();
                if (name.isEmpty()) return;
                includedKeywords.remove(name);
                excludedKeywords.remove(name);
                if (item->checkState(0) == Qt::Checked) includedKeywords.insert(name);
                else if (item->checkState(0) == Qt::PartiallyChecked)
                    excludedKeywords.insert(name);
                styleKeywordItem(item);
                runSearch();
            });

    connect(keywordTree, &QTreeWidget::itemClicked, this,
            [this](QTreeWidgetItem *item, int) {
                const bool opt = QApplication::keyboardModifiers() & Qt::AltModifier;
                const bool boxHandledIt = keywordCheckJustChanged;
                keywordCheckJustChanged = false;

                if (opt) {
                    /* Opt always means exclude, wherever in the row it was clicked. When
                       the indicator took the click first, Qt has just made it Checked;
                       overriding that is what the modifier asked for. */
                    setKeywordState(item, item->checkState(0) == Qt::PartiallyChecked
                                              ? Qt::Unchecked : Qt::PartiallyChecked);
                    return;
                }
                if (boxHandledIt) return;       // Qt already toggled it; nothing to do

                // clicked the text rather than the box
                setKeywordState(item, item->checkState(0) == Qt::Unchecked
                                          ? Qt::Checked : Qt::Unchecked);
            });

    /* The discoverable path to the same three states: a modifier nobody is told about is
       not a feature. */
    keywordTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(keywordTree, &QTreeWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) {
                QTreeWidgetItem *item = keywordTree->itemAt(pos);
                if (!item) return;
                QMenu menu(this);
                QAction *inc = menu.addAction("Include");
                QAction *exc = menu.addAction("Exclude");
                QAction *clr = menu.addAction("Clear");
                inc->setCheckable(true);
                exc->setCheckable(true);
                inc->setChecked(item->checkState(0) == Qt::Checked);
                exc->setChecked(item->checkState(0) == Qt::PartiallyChecked);
                QAction *chosen = menu.exec(keywordTree->viewport()->mapToGlobal(pos));
                if (chosen == inc)      setKeywordState(item, Qt::Checked);
                else if (chosen == exc) setKeywordState(item, Qt::PartiallyChecked);
                else if (chosen == clr) setKeywordState(item, Qt::Unchecked);
            });

    connect(loadBtn, &QPushButton::clicked, this, [this]{
        if (!results.isEmpty()) emit loadResults(results, false);
    });

    connect(addBtn, &QPushButton::clicked, this, [this]{
        if (!results.isEmpty()) emit loadResults(results, true);
    });

    connect(manageRootsBtn, &QPushButton::clicked, this,
            &CatalogView::manageRootsRequested);

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

    /* Only the SEARCH half is disabled. Manage stays live even with an empty catalog --
       it is how the user fills it, and disabling it would leave no way out of "nothing is
       catalogued yet". It is disabled only when the database will not open, where
       indexing genuinely cannot achieve anything. */
    searchEdit->setEnabled(usable);
    minRating->setEnabled(usable);
    keywordTree->setEnabled(usable);
    manageRootsBtn->setEnabled(open && !scanning);

    if (!open) {
        unavailableLabel->setText(
            "The catalog is unavailable -- the local index database could not be "
            "opened. Browsing is unaffected; searching is not available until it can "
            "be rebuilt.");
    }
    else if (n == 0) {
        unavailableLabel->setText(
            "Nothing is catalogued yet. Folders are catalogued as you open them, or "
            "press Manage to choose folders to index in the background.");
    }
    unavailableLabel->setVisible(!usable);

    /* Always says what is indexed, whether or not a search has been run: a search that
       found nothing means something quite different when the catalog is empty, and the
       user should not have to open a dialog to find out which case they are in. */
    if (!open) catalogStatusLabel->setText("Catalog unavailable.");
    else if (scanning) catalogStatusLabel->setText("Scanning...");
    else catalogStatusLabel->setText(
        QString("%1 %2 catalogued in %3 %4.")
            .arg(n)
            .arg(n == 1 ? "image" : "images")
            .arg(Catalog::instance().folderCount())
            .arg(Catalog::instance().folderCount() == 1 ? "folder" : "folders"));

    if (!usable) {
        resultLabel->clear();
        loadBtn->setEnabled(false);
        addBtn->setEnabled(false);
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
    rebuildKeywordList();
    /* Re-run whatever is in the box, so the counts reflect a catalog that just grew. */
    if (searchEdit->isEnabled()) runSearch();
}

void CatalogView::styleKeywordItem(QTreeWidgetItem *item) const
{
/*
    One row's appearance, from its state. Two independent signals that must COMPOSE
    rather than compete: exclusion is a font (strikethrough), ambiguity is a colour, so an
    excluded ambiguous keyword still reads as both.
*/
    const QString name = item->data(0, kNameRole).toString();
    const bool excluded = item->checkState(0) == Qt::PartiallyChecked;
    const bool ambiguous = ambiguousKeywords.contains(keywordFold(name));

    QFont f = keywordTree->font();
    f.setStrikeOut(excluded);
    item->setFont(0, f);

    if (excluded) item->setForeground(0, QBrush(QColor(0xd0, 0x60, 0x60)));
    else if (ambiguous) item->setForeground(0, QBrush(QColor(0xd0, 0xa0, 0x40)));
    else item->setForeground(0, QBrush(G::textColor));
}

void CatalogView::setKeywordState(QTreeWidgetItem *item, Qt::CheckState state)
{
    if (!item) return;
    const QString name = item->data(0, kNameRole).toString();
    if (name.isEmpty()) return;

    includedKeywords.remove(name);
    excludedKeywords.remove(name);
    if (state == Qt::Checked) includedKeywords.insert(name);
    else if (state == Qt::PartiallyChecked) excludedKeywords.insert(name);

    QSignalBlocker block(keywordTree);      // this is the handler, not a fresh edit
    item->setCheckState(0, state);
    styleKeywordItem(item);
    runSearch();
}

void CatalogView::rebuildKeywordList()
{
/*
    Render the keyword vocabulary as a FLAT list.

    There is no tree to build because there is no hierarchy left to render: a path like
    "Location|Canada|BC" is flattened into three ordinary keywords before it is ever
    indexed (Metadata/keywordflatten.h), so "Canada" is a keyword in its own right rather
    than a node to expand. That is also why the counts need no summing -- an ancestor name
    is linked directly to every image beneath it.

    AMBIGUITY IS THE ONE THING FLATTENING LOSES, so it is the one thing this marks: a name
    the catalog has seen under more than one parent is coloured and lists its parents in
    its tooltip, which is what tells the user that "Vancouver" is two places before they
    decide what to exclude.

    THE SELECTION SURVIVES A REBUILD because it lives in includedKeywords /
    excludedKeywords rather than in the widget. A refresh after a folder load must not
    silently drop the filter the user is looking at.
*/
    /* Building the list sets a check state on every row, and each one would fire
       itemChanged and re-run the search. */
    const QSignalBlocker block(keywordTree);
    keywordTree->clear();
    if (!Catalog::instance().isAvailable()) {
        ambiguousKeywords.clear();
        return;
    }

    ambiguousKeywords = Catalog::instance().ambiguousKeywords();

    /* Names that no longer exist in the catalog cannot go on restricting the search --
       the restriction would be invisible, because the row that described it is gone. */
    QSet<QString> live;

    for (const CatalogKeyword &k : Catalog::instance().keywords()) {
        live.insert(k.name);

        QTreeWidgetItem *item = new QTreeWidgetItem(keywordTree);
        item->setText(0, QString("%1  (%2)").arg(k.name).arg(k.count));
        item->setData(0, kNameRole, k.name);

        Qt::CheckState state = Qt::Unchecked;
        if (includedKeywords.contains(k.name)) state = Qt::Checked;
        else if (excludedKeywords.contains(k.name)) state = Qt::PartiallyChecked;
        item->setCheckState(0, state);

        QString tip = k.contexts.size() > 1
            ? QString("\"%1\" is used under more than one parent:\n    %2\n\n"
                      "Include it and exclude a parent to narrow it down.")
                  .arg(k.name, k.contexts.join("\n    "))
            : QString("Click to include. Opt+click to exclude.");
        item->setToolTip(0, tip);

        styleKeywordItem(item);
    }

    includedKeywords.intersect(live);
    excludedKeywords.intersect(live);

    keywordTree->sortItems(0, Qt::AscendingOrder);
}

CatalogQuery CatalogView::currentQuery() const
{
    CatalogQuery q;
    q.text = searchEdit->text();
    q.keywords = QStringList(includedKeywords.begin(), includedKeywords.end());
    q.excludeKeywords = QStringList(excludedKeywords.begin(), excludedKeywords.end());
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
    if (q.text.trimmed().isEmpty() && q.keywords.isEmpty()
        && q.excludeKeywords.isEmpty() && q.minRating == 0) {
        results.clear();
        totalMatches = 0;
        resultLabel->setText(QString("%1 images catalogued.")
                                 .arg(Catalog::instance().count()));
        loadBtn->setEnabled(false);
        addBtn->setEnabled(false);
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
    addBtn->setEnabled(!results.isEmpty());
}

void CatalogView::setScanning(bool on)
{
/*
    The panel only REPORTS a scan now -- starting and stopping one is in the Catalogued
    Folders dialog. Manage is disabled while a scan runs for the same reason the dialog
    freezes its own list: editing the roots underneath a running scan would leave the user
    unsure whether the folder they just added is being scanned.
*/
    scanning = on;
    setAvailability();
}
