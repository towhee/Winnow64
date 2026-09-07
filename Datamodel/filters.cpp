#include "Datamodel/filters.h"
#include "Cache/catalog.h"
#include "Metadata/keywordpaths.h"
#include "Main/global.h"
#include "Utilities/htmlwindow.h"
#include <QStyleFactory>
#include <functional>

/*
    OVERVIEW

    The Filters QTreeWidget is part of the filterDock - a window that includes:

    • filterTitleBar with several buttons
    • msgFrame with filterLabel and bfProgressBar
    • filters (the tree showing all the filter criterea)

    The filterDock is created in MW::createFilterDock in initialize.cpp

    When a very large folder is loaded and filters is selected, if not all metadata
    has been loaded, then the loading progress is signalled from MetaRead or DataModel
    to Filters::updateProgress.
*/

class FiltersDelegate : public QStyledItemDelegate
{
public:
    explicit FiltersDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) { }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex  &/*index*/) const
    {
        int height = qRound(G::strFontSize.toInt() * 1.7 * G::ptToPx);
        return QSize(option.rect.width(), height);
    }
};

Filters::Filters(QWidget *parent) : QTreeWidget(parent)
{
/*
    Used to define criteria for filtering the datamodel, based on which items are checked
    in the tree.

    The tree contains top level items (Categories ie Ratings, Color Classes, File types
    ...). For each top level item the children are the filter choices to filter
    DataModel->Proxy (dm->sf). The categories are divided into predefined (Search)
    and dynamic categories based on existing metadata (Ratings, Labels, File types,
    Camera Models, Focal Lengths, Titles etc).

    The tree columns are:
        0   CheckBox filter item (includes name)
        1   The value to filter (hidden)
        2   The number of proxy rows containing the value
        3   The number of datamodel rows containing the value

    The dynamic filter options are populated by DataModel on demand when the user filters
    or the filters dock has focus.

    The actual filtering is executed in SortFilter subclass of QSortFilterProxy (sf) in
    DataModel.

    The QTreeWidget does not support solo mode, so the decoration tree expansion is
    dissabled, and expansion/collapse is executed in the mousePressEvent.  The
    header row labels are modified with an icon to replicate the decoration arrow
    heads.

    When a criteria item checkbox is clicked:

        • Signals MW::filterChange.
        • Load all metadata if not already done.
        • SortFilter::filterChange() triggers SortFilter::filterAcceptsRow for all rows
          in the DataModel.
        • Each DataModel row is compared to all the checked filter items from QTreeWidget,
          updating the proxy model dm->sf.
        • BuildFilters updates the filtered item counts in QTreeWidget.
        • The proxy dm->sf is resorted.
        • The status bar is updated.
        • If dm->sf contains dm->currentDmIdx then scroll to it, else set to zero.
        • Rebuild the image cache.
        • Scroll to current.

    Category and item behavior


*/
    if (G::isLogger) G::log("Filters::Filters");
    viewport()->setObjectName("FiltersViewport");
    setRootIsDecorated(false);
    setSelectionMode(QAbstractItemView::NoSelection);
    setColumnCount(4);
//    setHeaderHidden(true);
    setColumnWidth(0, 250); // chkBox + description
    setColumnWidth(1, 50);  // value to filter (hidden)
    setColumnWidth(2, 50);  // number of proxy rows containing the value (filtered)
    setColumnWidth(3, 50);  // number of datamodel rows containing the value

    // Headerlabel set in search header: {"", "Value", "Filter", "Raw+Jpg", "All"}
    header()->setDefaultAlignment(Qt::AlignCenter);
    QStringList hdrLabels = {"", "Value", "Filter", "All"};
    this->setHeaderLabels(hdrLabels);
    /* how to add pixmap to a header
    model()->setHeaderData(0, Qt::Horizontal, QVariant::fromValue(QIcon(":/images/branch-closed-winnow.png")), Qt::DecorationRole);
    */

    // Cannot hide columns until tree fully initialized - see resizeColumns

    indentation = 14;
    setIndentation(indentation);

    hdrIsFilteringColor = QColor(Qt::yellow);
    hdrIsEmptyColor = G::disabledColor;
    /* Red reads as "taken out", and does not collide with the yellow a filtering
       category header uses, which sits on a different row. (An amber "ambiguous keyword"
       colour lived here too, until path identity made ambiguity impossible.) */
    itemIsExcludedColor = QColor(0xd0, 0x60, 0x60);

    int a = G::backgroundShade + 5;
    int b = G::backgroundShade - 15;

    categoryBackground.setStart(0, 0);
    categoryBackground.setFinalStop(0, 18);
    categoryBackground.setColorAt(0, QColor(a,a,a));
    categoryBackground.setColorAt(1, QColor(b,b,b));
    categoryFont = this->font();

    /*  The prompt in the Search category row when there is no query. It is also what
        "no search" IS: ignoreSearchStrings is what DataModel::searchStringChange tests,
        so the placeholder text must stay in that list. */
    enterSearchString = "Enter search query";
    ignoreSearchStrings << "" << enterSearchString << enterSearchString.toLower();
    int c = G::textShade + 15;
    int d = G::textShade - 15;
    searchDefaultTextColor = QColor(c,d,c);
    searchDefaultTextFont = font();
    searchDefaultTextFont.setItalic(true);

    filterCategoryToDmColumn[catSearch] = G::SearchColumn;

    filterCategoryToDmColumn[catPick] = G::PickColumn;
    filterCategoryToDmColumn[catRating] = G::RatingColumn;
    filterCategoryToDmColumn[catLabel] = G::LabelColumn;
    filterCategoryToDmColumn[catType] = G::TypeColumn;
    filterCategoryToDmColumn[catFolder] = G::FolderNameColumn;
    filterCategoryToDmColumn[catYear] = G::YearColumn;
    filterCategoryToDmColumn[catMonth] = G::MonthColumn;
    filterCategoryToDmColumn[catDay] = G::DayColumn;
    filterCategoryToDmColumn[catModel] = G::CameraModelColumn;
    filterCategoryToDmColumn[catLens] = G::LensColumn;
    filterCategoryToDmColumn[catFocalLength] = G::FocalLengthColumn;
    filterCategoryToDmColumn[catIso] = G::ISOColumn;
    filterCategoryToDmColumn[catTitle] = G::TitleColumn;
    /* The FLAT vocabulary (leaves + every hierarchy node), not the literal dc:subject, so
       a Lightroom tag written both ways is one filter item and an ancestor is filterable
       in its own right. See Metadata/keywordpaths.h. */
    filterCategoryToDmColumn[catKeyword] = G::KeywordsAllColumn;
    filterCategoryToDmColumn[catCreator] = G::CreatorColumn;
    filterCategoryToDmColumn[catAvailability] = G::AvailabilityColumn;
    // filterCategoryToDmColumn[catMissingThumbs] = G::MissingThumbColumn;
    filterCategoryToDmColumn[catCompare] = G::CompareColumn;

    createPredefinedFilters();
    createDynamicFilters();
    setCategoryBackground(a, b);

    setItemDelegate(new FiltersDelegate(this));

#ifdef Q_OS_WIN
    // The native Windows widget style draws a 1px vertical separator at the
    // column boundary in tree bodies; the macOS style does not. Fusion matches
    // the Mac behaviour (no separator). The app-wide stylesheet still applies on
    // top, so the rest of the appearance is unchanged.
    if (QStyle *fusion = QStyleFactory::create("Fusion")) {
        fusion->setParent(this);
        setStyle(fusion);
    }
#endif

    setFocusPolicy(Qt::NoFocus);

    /* Sits above the filters QTreeWidget and is used to message that the filters are
       being rebuilt.  Set invisible at start and rendered visible when building filters */
    filterLabel = new QLabel;
    filterLabel->setWordWrap(true);
    filterLabel->setText(buildingFiltersMsg);
    filterLabel->setAlignment(Qt::AlignCenter);
    filterLabel->setStyleSheet("QLabel {color:cadetblue;}");
    filterLabel->setVisible(false);

    bfProgressBar = new QProgressBar;
    bfProgressBar->setFixedHeight(6);
    bfProgressBar->setTextVisible(false);
    setProgressBarStyle();
    bfProgressBar->setValue(0);

    disableAllHeaders(true);

    debugFilters = false;

    connect(this, &Filters::itemClicked, this, &Filters::itemClickedSignal);
}

void Filters::createPredefinedFilters()
{
/*
    Predefined filters are edited by the user: Search only.
*/
    if (G::isLogger) G::log("Filters::createPredefinedFilters");
    if (debugFilters)
        qDebug() << "Filters::createPredefinedFilters"
                    ;
    search = new QTreeWidgetItem(this);
    search->setText(0, "Search");
    search->setFont(0, categoryFont);
    search->setIcon(0, QIcon(":/images/branch-closed-winnow.png"));
    search->setTextAlignment(4, Qt::AlignRight | Qt::AlignVCenter);
    search->setData(0, G::ColumnRole, G::SearchColumn);

    searchTrue = new QTreeWidgetItem(search);
    searchTrue->setText(0, enterSearchString);
    searchTrue->setCheckState(0, Qt::Checked);
    /*  bool, matching what the SearchColumn now holds -- searchFalse below was
        already a bool, so the two halves of one category disagreed. QVariant
        comparison converts between the two, so this was never a live matching
        bug (measured, see "Settling Search and Ingested" in Documentation.txt);
        it is settled so that nothing has to keep relying on that. */
    searchTrue->setData(1, Qt::EditRole, true);
    searchTrue->setFlags(searchTrue->flags() | Qt::ItemIsEditable);
    searchTrue->setFont(0, searchDefaultTextFont);
    searchTrue->setForeground(0, searchDefaultTextColor);
    searchTrue->setToolTip(0,
        "Click to type a query, then press Return to run it.\n\n"
        "Words are AND-ed. Use OR between alternatives, (brackets) to group,\n"
        "\"quotes\" for a phrase, and -word or NOT word to leave something out.\n\n"
        "Text is not case sensitive; OR, AND and NOT are operators only in capitals.\n"
        "Click ? in the Filters title bar for the full syntax.\n\n"
        "Right-click for a larger editing window, and to save and load queries.");
    searchTrueIdx = indexFromItem(searchTrue, 0);

    searchFalse = new QTreeWidgetItem(search);
    searchFalse->setText(0, "No match");
    searchFalse->setCheckState(0, Qt::Unchecked);
    searchFalse->setData(1, Qt::EditRole, false);
}

void Filters::createFilter(QTreeWidgetItem *cat, QString name)
{
    if (G::isLogger) G::log("Filters::createFilter", name);
    if (debugFilters)
        qDebug() << "Filters::createFilter"
                 << "name =" << name
                 << "cat =" << cat->text(0)
                    ;
    cat->setText(0, name);
    cat->setData(0, G::ColumnRole, filterCategoryToDmColumn[name]);
    cat->setIcon(0, QIcon(":/images/branch-closed-winnow.png"));
}

void Filters::createDynamicFilters()
{
/*
    Dynamic filters change with the model data and apply to file metadata such as file
    type, camera model etc. When a new folder is selected each dynamic filter column is
    scanned for unique elements which are added to the dynamic filter by
    addCategoryFromData.
*/
    if (G::isLogger) G::log("Filters::createDynamicFilters");
    if (debugFilters)
        qDebug() << "Filters::createDynamicFilters"
                    ;

    picks = new QTreeWidgetItem(this);
    ratings = new QTreeWidgetItem(this);
    labels = new QTreeWidgetItem(this);
    types = new QTreeWidgetItem(this);
    folders = new QTreeWidgetItem(this);
    years = new QTreeWidgetItem(this);
    months = new QTreeWidgetItem(this);
    days = new QTreeWidgetItem(this);
    models = new QTreeWidgetItem(this);
    lenses = new QTreeWidgetItem(this);
    focalLengths = new QTreeWidgetItem(this);
    isos = new QTreeWidgetItem(this);
    titles = new QTreeWidgetItem(this);
    keywords = new QTreeWidgetItem(this);
    creators = new QTreeWidgetItem(this);
    availability = new QTreeWidgetItem(this);
    // missingThumbs = new QTreeWidgetItem(this);
    compare = new QTreeWidgetItem(this);

    createFilter(picks, catPick);
    createFilter(ratings, catRating);
    createFilter(labels, catLabel);
    createFilter(types, catType);
    createFilter(folders, catFolder);
    createFilter(years, catYear);
    createFilter(months, catMonth);
    createFilter(days, catDay);
    createFilter(models, catModel);
    createFilter(lenses, catLens);
    createFilter(focalLengths, catFocalLength);
    createFilter(isos, catIso);
    createFilter(titles, catTitle);
    createFilter(keywords, catKeyword);
    createFilter(creators, catCreator);
    createFilter(availability, catAvailability);
    // createFilter(missingThumbs, catMissingThumbs);
    createFilter(compare, catCompare);
}

void Filters::setCategoryBackground(QTreeWidgetItem *cat)
{
    if (G::isLogger) G::log("Filters::setCategoryBackground(QTreeWidgetItem *cat)");
    if (debugFilters)
        qDebug() << "Filters::setCategoryBackground"
                 << "cat =" << cat->text(0)
                    ;

    cat->setBackground(0, categoryBackground);
    cat->setBackground(2, categoryBackground);
    cat->setBackground(3, categoryBackground);
}

void Filters::setCategoryBackground(const int &a, const int &b)
{
/*
    Sets the background gradient for the category items. This function is also called when the
    user changes the background shade in preferences.
*/
    if (G::isLogger) G::log("Filters::setCategoryBackground");
    if (debugFilters)
        qDebug() << "Filters::setCategoryBackground(const int &a, const int &b)"
                    ;
    categoryBackground.setColorAt(0, QColor(a,a,a));
    categoryBackground.setColorAt(1, QColor(b,b,b));

    setCategoryBackground(search);
    setCategoryBackground(picks);
    setCategoryBackground(ratings);
    setCategoryBackground(labels);
    setCategoryBackground(types);
    setCategoryBackground(folders);
    setCategoryBackground(years);
    setCategoryBackground(months);
    setCategoryBackground(days);
    setCategoryBackground(models);
    setCategoryBackground(lenses);
    setCategoryBackground(focalLengths);
    setCategoryBackground(isos);
    setCategoryBackground(titles);
    setCategoryBackground(keywords);
    setCategoryBackground(creators);
    setCategoryBackground(availability);
    // setCategoryBackground(missingThumbs);
    setCategoryBackground(compare);
}

void Filters::removeChildrenDynamicFilters()
{
/*
    The dynamic filters (see createDynamicFilters) are rebuilt when a new
    folder is selected.  This function removes any pre-existing children to
    prevent duplication and orphans.
*/
    if (G::isLogger || G::isFlowLogger) G::log("Filters::removeChildrenDynamicFilters");
    if (debugFilters)
        qDebug() << "Filters::removeChildrenDynamicFilters"
                    ;
    picks->takeChildren();
    ratings->takeChildren();
    labels->takeChildren();
    types->takeChildren();
    folders->takeChildren();
    years->takeChildren();
    months->takeChildren();
    days->takeChildren();
    models->takeChildren();
    lenses->takeChildren();
    focalLengths->takeChildren();
    isos->takeChildren();
    titles->takeChildren();
    keywords->takeChildren();
    creators->takeChildren();
    availability->takeChildren();
    // missingThumbs->takeChildren();
    compare->takeChildren();
}

void Filters::setPicksState(bool isChecked)
{
/*
    Quick Menu Filter toggles 'Picked' and 'Unpicked'.
    isChecked is the menu item status.
*/
    if (G::isLogger) G::log("Filters::checkPicks");
    if (debugFilters)
        qDebug() << "Filters::setPicksState" << isChecked
                    ;

    if (isChecked) {
        checkItem(picks, "Picked", Qt::Checked);
        checkItem(picks, "Unpicked", Qt::Unchecked);
        checkItem(picks, "Rejected", Qt::Unchecked);
    }
    else {
        checkItem(picks, "Picked", Qt::Unchecked);
        checkItem(picks, "Unpicked", Qt::Unchecked);
        checkItem(picks, "Rejected", Qt::Unchecked);
    }
    emit filterChange("Filters::checkPicks");
}

void Filters::setRatingState(QString rating, bool isChecked)
{
/*
    Quick Menu Filter rating by setting the rating true.
*/
    if (G::isLogger) G::log("Filters::checkRating");
    if (debugFilters)
        qDebug() << "Filters::checkRating"
                 << "rating =" << rating
                 << "isChecked =" << isChecked
                    ;
    Qt::CheckState state;
    isChecked ? state = Qt::Checked : state = Qt::Unchecked;
    for (int i = 0; i < ratings->childCount(); i++) {
        if (ratings->child(i)->text(0) == rating) {
            ratings->child(i)->setCheckState(0, state);
            styleFilterItem(ratings->child(i));
        }
    }
}

void Filters::setLabelState(QString label, bool isChecked)
{
/*
    Quick Menu Filter label by setting the rating true.
*/
    if (G::isLogger) G::log("Filters::checkRating");
    if (debugFilters)
        qDebug() << "Filters::checkLabel"
                 << "rating =" << label
                 << "isChecked =" << isChecked
                    ;
    Qt::CheckState state;
    isChecked ? state = Qt::Checked : state = Qt::Unchecked;
    for (int i = 0; i < labels->childCount(); i++) {
        if (labels->child(i)->text(0) == label) {
            labels->child(i)->setCheckState(0, state);
            styleFilterItem(labels->child(i));
        }
    }
}

bool Filters::isRatingChecked(QString rating)
{
    if (G::isLogger) G::log("Filters::checkRating");
    if (debugFilters)
        qDebug() << "Filters::isRatingChecked"
                 << "rating =" << rating
                    ;
    for (int i = 0; i < ratings->childCount(); i++) {
        if (ratings->child(i)->text(0) == rating) {
            if (ratings->child(i)->checkState(0) == Qt::Checked) return true;
            else return false;
        }
    }

    return false;
}

bool Filters::isLabelChecked(QString label)
{
    if (G::isLogger) G::log("Filters::checkRating");
    if (debugFilters)
        qDebug() << "Filters::isLabelChecked"
                 << "label =" << label
                    ;
    for (int i = 0; i < labels->childCount(); i++) {
        if (labels->child(i)->text(0) == label) {
            if (labels->child(i)->checkState(0) == Qt::Checked) return true;
            else return false;
        }
    }
    // not found
    return false;
}

bool Filters::isTitleChecked(QString title)
{
    if (G::isLogger) G::log("Filters::isTitleChecked");
    if (debugFilters)
        qDebug() << "Filters::isTitleChecked"
                 << "title =" << title
            ;
    for (int i = 0; i < titles->childCount(); i++) {
        if (titles->child(i)->text(0) == title) {
            if (titles->child(i)->checkState(0) == Qt::Checked) return true;
            else return false;
        }
    }
    // not found
    return false;
}

bool Filters::isCreatorChecked(QString creator)
{
    if (G::isLogger) G::log("Filters::isCreatorChecked");
    if (debugFilters)
        qDebug() << "Filters::isCreatorChecked"
                 << "creator =" << creator
            ;
    for (int i = 0; i < creators->childCount(); i++) {
        if (creators->child(i)->text(0) == creator) {
            if (creators->child(i)->checkState(0) == Qt::Checked) return true;
            else return false;
        }
    }
    // not found
    return false;
}

bool Filters::isAnyCatItemChecked(QTreeWidgetItem *category)
{
    if (G::isLogger) G::log("Filters::isAnyLabelChecked");
    for (int i = 0; i < category->childCount(); ++i) {
        if (debugFilters)
        qDebug() << "Filters::isAnyCatItemChecked"
                 << "label =" << category->child(i)->text(0)
                 << "checkState =" << category->child(i)->checkState(0)
            ;
        if (category->child(i)->checkState(0) == Qt::Checked) return true;
    }
    // nothing checked
    return false;
}

bool Filters::isAnyFilter()
{
/*
    This is used to determine the filter status in MW::updateFilterStatus
*/
    if (G::isLogger) G::log("Filters::isAnyFilter");
    if (debugFilters)
        qDebug() << "Filters::isAnyFilter"
            ;
    QMutexLocker locker(&mutex);

    QTreeWidgetItemIterator it(this);
    if (searchTrue->checkState(0) == Qt::Checked && searchTrue->text(0) != enterSearchString)
        return true;
    while (*it) {
        if ((*it)->parent() && (*it) != searchTrue) {
            /* Either state filters. An exclusion narrows the set just as an inclusion
               does, so a panel holding only exclusions must still report "filtered" --
               or the status bar says nothing is filtered while rows are missing. */
            if ((*it)->checkState(0) != Qt::Unchecked) return true;
        }
        ++it;
    }
    return false;
}

bool Filters::isOnlyMostRecentDayChecked()
{
/*
    This is used to sync MW filters menu check state with Filters panel (here)
*/
    if (G::isLogger) G::log("Filters::isOnlyMostRecentDayChecked");
    if (debugFilters)
        qDebug() << "Filters::isOnlyMostRecentDayChecked"
                    ;
    int n = days->childCount();
    for (int i = 0; i < n; i++) {
        /* Qt::Checked only: an EXCLUDED day (PartiallyChecked) is not a day the user
           asked to see, and would otherwise read as one because it is non-zero. */
        bool isChecked = days->child(i)->checkState(0) == Qt::Checked;
        if ((i < n - 1) && isChecked) return false;
         if (i == n - 1) return isChecked;
    }
    return false;
}

void Filters::setSearchNewFolder()
{
    if (G::isLogger) G::log("Filters::setSearchNewFolder");
    if (debugFilters)
        qDebug() << "Filters::setSearchNewFolder"
                    ;
    searchTrue->setText(0, enterSearchString);
    searchTrue->setCheckState(0, Qt::Checked);
}

void Filters::setCategoryFilterStatus(QTreeWidgetItem *item)
{
    // used?
    if (G::isLogger) G::log("Filters::setCategoryFilterStatus");
    if (debugFilters)
        qDebug() << "Filters::setCategoryFilterStatus"
                    ;

    if (!item->parent()) return;

    // is this category filtering after itemCheckStateHasChanged
    if (isCatFiltering(item->parent())) {
        item->parent()->setForeground(0, QBrush(hdrIsFilteringColor));
    }
    else {
        item->parent()->setForeground(0, QBrush(G::textColor));
    }
}

bool Filters::isPredefinedNonZeroCount(QString itemName)
{
    if (G::isLogger) G::log("Filters::isPredefinedZeroCount");

    bool isNonZero = false;
    QList<QTreeWidgetItem*> items {picks, ratings, labels};
    for (QTreeWidgetItem *item : items) {
        for (int i = 0; i < item->childCount(); i++) {
            if (item->child(i)->text(0) == itemName) {
                if (item->child(i)->text(2) != "0") {
                    isNonZero = true;
                    break;
                }
            }
        }
    }
    return isNonZero;
}

void Filters::disableColorZeroCountItems()
{
    // not being used
    if (G::isLogger) G::log("Filters::disableColorZeroCountItems");
    if (debugFilters)
        qDebug() << "Filters::disableColorZeroCountItems"
                    ;

    QMutexLocker locker(&mutex);

    QTreeWidgetItemIterator it(this);
    while (*it) {
        if ((*it)->parent() && (*it)->parent() != search) {
           if ((*it)->text(2) == "0") (*it)->setForeground(0, QBrush(G::disabledColor));
            else (*it)->setForeground(0, QBrush(G::textColor));
        }
        ++it;
    }
}

void Filters::disableAllItems(bool disable)
{
    // not used
    if (G::isLogger) G::log("Filters::disableAllItems");
    if (debugFilters)
    qDebug() << "Filters::disableAllItems" << disable;
    // QMutexLocker locker(&mutex);
    mutex.lock();

    QTreeWidgetItemIterator it(this);
    while (*it) {
        if (disable) (*it)->setForeground(0, QBrush(G::disabledColor));
        else (*it)->setForeground(0, QBrush(G::textColor));
         ++it;
    }
    mutex.unlock();
    if (!disable) setEachCatTextColor();
}

void Filters::disableAllHeaders(bool disable)
{
    if (G::isLogger) G::log("Filters::disableAllHeaders");
    if (debugFilters)
        qDebug() << "Filters::disableAllHeaders"
                    ;
    QMutexLocker locker(&mutex);

    QTreeWidgetItemIterator it(this);
    while (*it) {
        if (!(*it)->parent()) {
            if (disable) (*it)->setForeground(0, QBrush(G::disabledColor));
            else {
                if (isCatFiltering(*it))
                    (*it)->setForeground(0, QBrush(G::disabledColor));
                else
                    (*it)->setForeground(0, QBrush(G::textColor));
            }
        }
        ++it;
    }
}

void Filters::disableColorAllHeaders(bool disable)
{
    // not used
    if (G::isLogger) G::log("Filters::disableColorAllHeaders");
    if (debugFilters)
        qDebug() << "Filters::disableColorAllHeaders"
            ;
    QMutexLocker locker(&mutex);

    QTreeWidgetItemIterator it(this);
    while (*it) {
        if (!(*it)->parent() && (*it) != search) {
            if (disable) (*it)->setForeground(0, QBrush(G::disabledColor));
            else (*it)->setForeground(0, QBrush(G::textColor));
        }
        ++it;
    }
}

void Filters::updateProgress(int progress)
{
    // if (G::isLogger) G::log("Filters::updateProgress");
    if (debugFilters)
    {
        qDebug() << "Filters::updateProgress" << progress;
    }
    bfProgressBar->setValue(progress);
}

void Filters::setProgressBarStyle()
{
//    bfProgressBar->setStyleSheet("QProgressBar::chunk{background-color:cadetblue;}");
}

void Filters::setEachCatTextColor()
{
/*
    Update all categories 'is filtering' status
*/
    if (G::isLogger) G::log("Filters::setEachCatTextColor");
    if (debugFilters)
        qDebug() << "Filters::setEachCatTextColor"
                    ;

    QMutexLocker locker(&mutex);

    QTreeWidgetItemIterator it(this);
    while (*it) {
        if (!(*it)->parent() && (*it) != search) {
            if ((*it)->childCount() == 0)
                (*it)->setForeground(0, QBrush(hdrIsEmptyColor));
            // category only has one item and item not "true"
            else if ((*it)->childCount() == 1 && (*it)->child(0)->text(0) != "true")
                (*it)->setForeground(0, QBrush(hdrIsEmptyColor));
            else {
                bool isChecked = false;
                for (int i = 0; i < (*it)->childCount(); i++) {
                    /* Includes and excludes alike: the header says whether the category
                       is doing anything, and an exclusion is doing something. */
                    if ((*it)->child(i)->checkState(0) != Qt::Unchecked) {
                        isChecked = true;
                        break;
                    }
                }
                QColor colorToUse;
                isChecked ? colorToUse = hdrIsFilteringColor : colorToUse = G::textColor;
                (*it)->setForeground(0, QBrush(colorToUse));
             }
        }
        ++it;
    }

    search->setForeground(0, G::textColor);
    if (searchTrue->text(0) != enterSearchString) {
        if (searchTrue->checkState(0) == Qt::Checked)
            search->setForeground(0, QBrush(hdrIsFilteringColor));
        if (searchFalse->checkState(0) == Qt::Checked)
            search->setForeground(0, QBrush(hdrIsFilteringColor));
    }
}

bool Filters::isCatFiltering(QTreeWidgetItem *item)
{
/*
    This is used to determine if the category (item) has any children with a checkbox
    set true.  It is used to change the category header so the user can tell which
    categories have filters engaged.

    If there is only one child item then it does not matter whether it is set, as
    there will be no filtering because the one item represents the entire population.
*/
    if (G::isLogger) G::log("Filters::isCatFiltering");
    /*
    if (debugFilters)
        qDebug() << "Filters::isCatFiltering"
                 << "Category =" << item->text(0)
                    ;  //*/
    if (item == search) {
        if (searchTrue->text(0) != enterSearchString) {
          if (searchTrue->checkState(0) == Qt::Checked) return true;
          if (searchFalse->checkState(0) == Qt::Checked) return true;
        }
        else return false;
    }
    if (item->childCount() > 1) {
        for (int i = 0; i < item->childCount(); i++) {
            if (item->child(i)->checkState(0) != Qt::Unchecked) return true;
        }
    }
    return false;
}

void Filters::disableEmptyCat()
{
/*

*/
    // used?
    if (G::isLogger) G::log("Filters::isCatEmpty");
    if (debugFilters)
        qDebug() << "Filters::disableEmptyCat"
                    ;
    QTreeWidgetItemIterator it(this);
    while (*it) {
        // categories
        if (!(*it)->parent() && (*it) != search) {
            //qDebug() << (*it)->text(0) << (*it)->childCount();
            if ((*it)->childCount() < 2)
                (*it)->setForeground(0, QBrush(hdrIsEmptyColor));
            else {
                (*it)->setForeground(0, G::textColor);
            }
        }
        ++it;
    }
}

void Filters::enable() {
    if (G::isLogger) G::log("Filters::enable");
    setEnabled(true);
    disableAllItems(false);
    setEachCatTextColor();
}

void Filters::disable() {
    if (G::isLogger) G::log("Filters::disable");
    setEnabled(false);
    disableAllItems(true);
}

void Filters::invertFilters()
{
    if (G::isLogger) G::log("Filters::invertFilters");
    if (debugFilters)
        qDebug() << "Filters::invertFilters"
                    ;
    QList<QString> catWithCheckedItems;
    QString cat = "";

    QMutexLocker locker(&mutex);

    QTreeWidgetItemIterator it(this);

    // enable all items
    // disableColorZeroCountItems();

    // populate catWithCheckedItems list with only categories that have one or more checked items
    while (*it) {
        // if no parent then it is a category
        if (!(*it)->parent()) {
            cat = (*it)->text(0);
//            qDebug() << (*it)->text(0);
        }
        // traverse the children of the category
        if ((*it)->parent()) {
            bool isChecked = (*it)->checkState(0) == Qt::Checked;
            if (isChecked && cat != "") {
                catWithCheckedItems.append((*it)->parent()->text(0));
                // prevent adding same category twice
                cat = "";
            }
        }
        ++it;
    }

   // invert categories with checked items
    QTreeWidgetItemIterator it2(this);
    while (*it2) {
        // traverse the children of the category and invert checkstate
        if ((*it2)->parent()) {
            // only want categories with checked items
            QString s = (*it2)->parent()->text(0);
            if (catWithCheckedItems.contains(s)) {
                // ignore items that do not exist in datamodel (column 3 has unfiltered count)
                if ((*it2)->text(3) != "0") {
                    /* Invert the INCLUSIONS only. An exclusion is left exactly as it is:
                       "show me everything except this" has no meaningful inverse, and
                       turning it into an inclusion would silently reverse what the user
                       asked for -- the one filter state where guessing is worse than
                       doing nothing. */
                    Qt::CheckState st = (*it2)->checkState(0);
                    if (st == Qt::PartiallyChecked) { ++it2; continue; }
                    (*it2)->setCheckState(0, st == Qt::Checked ? Qt::Unchecked
                                                               : Qt::Checked);
                }
            }
        }
        ++it2;
    }

    // disable items with no filter count
    // disableColorZeroCountItems();

    // emit filterChange();  // this is done in MW::invertFilters - which calls this function
}

void Filters::loadingDataModel(bool isLoaded)
{
    if (G::isLogger) G::log("Filters::loadingDataModel", "isLoaded = " + QVariant(isLoaded).toString());
    if (debugFilters)
        qDebug() << "Filters::loadingDataModel  isLoaded =" << isLoaded
                    ;
    if (isLoaded) {
        bfProgressBar->setValue(0);
        bfProgressBar->setVisible(false);
        msgFrame->setVisible(false);
        filterLabel->setText("");
        filterLabel->setVisible(false);
        setEnabled(true);
        // disableColorZeroCountItems();
    }
    else {
        msgFrame->setVisible(true);
        filterLabel->setText("Filters disabled while loading all metadata...");
        filterLabel->setVisible(true);
        bfProgressBar->setVisible(true);
        // Allow search to remain visible in case search selected in menu or F2 pressed
        // before or while filters are being built.
        collapseAllFiltersExceptSearch();
        setEnabled(false);
    }
}

void Filters::loadingDataModelFailed()
{
    msgFrame->setVisible(false);
    filterLabel->setText("");
    filterLabel->setVisible(false);
    setEnabled(false);
}

void Filters::startBuildFilters(bool isReset)
{
    if (G::isLogger || G::isFlowLogger) G::log("Filters::startBuildFilters");
    if (debugFilters)
        qDebug() << "Filters::startBuildFilters"
                 << "G::allMetadataAttempted =" << G::allMetadataAttempted
                    ;
    if (isReset) removeChildrenDynamicFilters();
    filtersBuilt = false;
    buildingFilters = true;
    if (!G::allMetadataAttempted) {
        msgFrame->setVisible(true);
        filterLabel->setText(buildingFiltersMsg);
        filterLabel->setVisible(true);
        //setProgressBarStyle();
        bfProgressBar->setVisible(true);
    }
    //if (isReset) collapseAll();
    disableAllHeaders(true);
    setEnabled(false);
    finishedBuildFilters();
}

void Filters::finishedBuildFilters()
{
    if (G::isLogger) G::log("Filters::finishedBuildFilters");
    if (debugFilters)
    {
        qDebug() << "Filters::finishedBuildFilters";
        qDebug() << "\n";
    }

    filtersBuilt = true;
    buildingFilters = false;
    filterLabel->setVisible(false);
    bfProgressBar->setValue(0);
    bfProgressBar->setVisible(false);
    msgFrame->setVisible(false);
    // disableColorZeroCountItems();
    setEnabled(true);
    setKeywordCategoryToolTip();
    //if (isSolo) collapseAll();
    //else expandAll();
}

void Filters::clearAll()
{
/*
    Uncheck all the filter items but do not signal filter change.  This is called when a new
    folder is selected to reset the filter criteria.
*/
    if (G::isLogger || G::isFlowLogger) G::log("Filters::clearAll");
    if (debugFilters)
        qDebug() << "Filters::clearAll"
                    ;
    QMutexLocker locker(&mutex);

    /* Nothing is checked any more, so there is nothing to range from. */
    rangeAnchorCategory = nullptr;
    rangeAnchorItem.clear();

    QTreeWidgetItemIterator it(this);
    while (*it) {
        if ((*it)->parent()) {
            (*it)->setCheckState(0, Qt::Unchecked);
            styleFilterItem(*it);       // drop an exclusion's strikethrough too
            (*it)->setData(2, Qt::EditRole, "");
            (*it)->setData(3, Qt::EditRole, "");
            (*it)->setData(4, Qt::EditRole, "");
            (*it)->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
            (*it)->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
        }
        else {
            (*it)->setForeground(0, QBrush(G::disabledColor));
            //(*it)->setForeground(0, QBrush(G::textColor));
        }
        ++it;
    }
    setSearchNewFolder();
}

bool Filters::otherHdrExpanded(QModelIndex thisIdx)
{
/*
    Determines if there is another category expanded in the tree.  This is used to
    control solo mode in mousePressEvent.
*/
    if (G::isLogger || G::isFlowLogger) G::log("Filters::otherHdrExpanded");
    if (debugFilters || G::isLogger || G::isFlowLogger)
        qDebug() << "Filters::otherHdrExpanded"
                    ;
    for (int i = 0; i < topLevelItemCount(); ++i) {
        QModelIndex idx = indexFromItem(topLevelItem(i));
        if (idx == thisIdx) continue;
        if (isExpanded(idx)) return true;
    }
    return false;
}

void Filters::reset()
{
/*
    Reset filters before loading a new datamodel.
*/
    if (G::isLogger || G::isFlowLogger) G::log("Filters::reset");

    // reset flags
    filtersBuilt = false;
    buildingFilters = false;
    isReset = true;  // req'd?
    // clear all items based on data content ie file types, camera model
    removeChildrenDynamicFilters();

    // reset all items to unchecked
    clearAll();

    // reset message frame
    // loadingDataModel(true);

    setEnabled(true);
    filterLabel->setVisible(false);
    activeCategory = nullptr;

}

bool Filters::isFilterableItem(QTreeWidgetItem *item) const
{
    if (!item || !item->parent() || item->isDisabled()) return false;
    /* The Search category's two rows are a text box and its negation, not values to be
       included or excluded -- "not matching the search text" is what searchFalse already
       is, so an exclusion there would be a second way to say the same thing. */
    if (item == searchTrue || item == searchFalse) return false;
    return true;
}

void Filters::styleFilterItem(QTreeWidgetItem *item)
{
/*
    One item's appearance, from its state.

    TWO SIGNALS THAT MUST COMPOSE rather than compete: exclusion is a FONT (struck
    through) and ambiguity is a COLOUR, so an excluded ambiguous keyword still reads as
    both. Had both been colours, one would have had to win and the user would lose the
    other fact exactly when they need it -- while resolving the ambiguity.
*/
    if (!item || !item->parent()) return;

    const bool excluded = item->checkState(0) == Qt::PartiallyChecked;

    QFont f = font();
    f.setStrikeOut(excluded);
    item->setFont(0, f);

    if (excluded) item->setForeground(0, QBrush(itemIsExcludedColor));
    else item->setForeground(0, QBrush(G::textColor));
}

void Filters::setItemFilterState(QTreeWidgetItem *item, Qt::CheckState state)
{
/*
    The one way an item's filter state changes, whatever gesture asked for it -- clicking
    the box, clicking the text, Opt+clicking, or the context menu.
*/
    if (!isFilterableItem(item)) return;
    if (G::isLogger) G::log("Filters::setItemFilterState", item->text(0));

    itemCheckStateHasChanged = false;
    item->setCheckState(0, state);
    styleFilterItem(item);
    if (state == Qt::Checked) noteRangeAnchor(item);
    activeCategory = item->parent();
    emit filterChange("Filters::setItemFilterState");
}

void Filters::noteRangeAnchor(QTreeWidgetItem *item)
{
/*
    Remember the item just checked. It is where the next Shift+click ranges FROM.
*/
    if (!item || !item->parent()) return;
    rangeAnchorCategory = item->parent();
    rangeAnchorItem = item->text(0);
}

bool Filters::applyRangeCheck(QTreeWidgetItem *item)
{
/*
    SHIFT+CLICK CHECKS A RANGE, the gesture every list in the OS uses for "and everything
    between". The range runs from the item last checked to the one clicked, and only
    within the SAME category: a range spanning Ratings into Color classes would cross an
    AND boundary and mean nothing the user asked for.

    The whole range is INCLUDED unless it is already all included, in which case the same
    gesture clears it -- so the gesture undoes itself and there is no separate way out. An
    EXCLUDED item counts as not included, so a range containing one is completed rather
    than emptied, and the exclusion is replaced by an inclusion like any other item in it.

    One filterChange is emitted for the whole range, not one per item: each emission
    re-runs the filter over the datamodel, and a fifty-item range would re-run it fifty
    times to arrive at the same place.
*/
    if (!isFilterableItem(item)) return false;
    QTreeWidgetItem *cat = item->parent();
    if (cat == nullptr || cat != rangeAnchorCategory) return false;

    int anchorRow = -1;
    for (int i = 0; i < cat->childCount(); i++) {
        if (cat->child(i)->text(0) == rangeAnchorItem) {
            anchorRow = i;
            break;
        }
    }
    /* The anchor item is gone -- the category was rebuilt since it was checked. Nothing
       to range from, so the caller treats this as an ordinary click. */
    if (anchorRow == -1) return false;

    const int clickedRow = cat->indexOfChild(item);
    const int first = qMin(anchorRow, clickedRow);
    const int last = qMax(anchorRow, clickedRow);

    bool allChecked = true;
    for (int i = first; i <= last; i++) {
        QTreeWidgetItem *child = cat->child(i);
        if (!isFilterableItem(child)) continue;
        if (child->checkState(0) != Qt::Checked) {
            allChecked = false;
            break;
        }
    }
    const Qt::CheckState state = allChecked ? Qt::Unchecked : Qt::Checked;

    for (int i = first; i <= last; i++) {
        QTreeWidgetItem *child = cat->child(i);
        if (!isFilterableItem(child)) continue;
        if (child->checkState(0) == state) continue;
        child->setCheckState(0, state);
        styleFilterItem(child);
    }

    noteRangeAnchor(item);
    activeCategory = cat;
    emit filterChange("Filters::applyRangeCheck");
    return true;
}

void Filters::setKeywordCategoryToolTip()
{
/*
    The Keywords category's tooltip. A separate function only because two places need it
    -- the end of a build and the end of a rebuild -- and it used to be carried along by
    the ambiguity refresh that lived here.

    THERE IS NO AMBIGUITY MARKING ANY MORE, and there is nothing left to mark: a keyword's
    identity is its full path (catalog schema 10), so "Vancouver" under Canada and
    "Vancouver" under USA are two separate items with two separate counts rather than one
    amber item meaning both. What used to need a colour, a role, a shared QSet and a
    catalog lookup is now simply two rows.
*/
    if (G::isLogger) G::log("Filters::setKeywordCategoryToolTip");

    keywords->setToolTip(0,
        "A keyword is its whole path, so the same name under two parents is two\n"
        "keywords with two counts.\n\n"
        "Every image is also linked to each ANCESTOR of its keywords, so including\n"
        "a parent finds everything beneath it.");

    /*  The per-item tooltips are set by addKeywordItems as each node is built, because a
        nested item's tooltip carries its full PATH -- which is how two keywords sharing a
        leaf are told apart in a tree that shows only leaves. */
}

void Filters::contextMenuEvent(QContextMenuEvent *event)
{
/*
    The discoverable route to the three filter states. Opt+click is the fast one and
    matches the modifier idiom used elsewhere, but a modifier nobody is told about is not
    a feature, so the same three choices are here by name.
*/
    QTreeWidgetItem *item = itemAt(event->pos());

    /*  The Search row is not a value to include or exclude, so it gets the query menu
        instead -- see showSearchQueryMenu. It is offered whether or not the filters have
        finished building: nothing on it reads the categories. */
    if (item != nullptr && item == searchTrue) {
        showSearchQueryMenu(event->globalPos());
        return;
    }

    const bool ready = categoriesFrom == FromCatalog
                       || (!G::isModifyingDatamodel && !buildingFilters);
    if (!isFilterableItem(item) || !ready) {
        /*  The dock's own actions, which the widget can no longer show for itself -- see
            addFilterActions. This is what a right-click anywhere else in the tree gives,
            and it must stay exactly that. */
        QMenu menu(this);
        addFilterActions(menu);
        menu.exec(event->globalPos());
        return;
    }

    const Qt::CheckState now = item->checkState(0);
    QMenu menu(this);
    QAction *inc = menu.addAction("Include");
    QAction *exc = menu.addAction("Exclude");
    QAction *clr = menu.addAction("Clear");
    inc->setCheckable(true);
    exc->setCheckable(true);
    inc->setChecked(now == Qt::Checked);
    exc->setChecked(now == Qt::PartiallyChecked);
    clr->setEnabled(now != Qt::Unchecked);
    addFilterActions(menu);

    QAction *chosen = menu.exec(event->globalPos());
    if (chosen == inc)      setItemFilterState(item, Qt::Checked);
    else if (chosen == exc) setItemFilterState(item, Qt::PartiallyChecked);
    else if (chosen == clr) setItemFilterState(item, Qt::Unchecked);
}

void Filters::addFilterActions(QMenu &menu)
{
/*
    Append the dock's own filter actions -- Clear all filters, Invert, the token editor,
    Expand/Collapse all, Solo -- under a separator.

    THE WIDGET CANNOT SHOW THEM FOR ITSELF ANY MORE, and that is the point. They used to
    be shown by Qt::ActionsContextMenu, which QWidget::event handles directly: with that
    policy contextMenuEvent is NEVER CALLED, so the Include/Exclude/Clear menu below and
    the Search row's query menu were unreachable however they were written -- every
    right-click in the tree produced the same action list. The policy is
    Qt::DefaultContextMenu now (MW::createFilterViewContextMenu) and every menu this
    class builds ends with these, so nothing was taken away.

    The actions run themselves: a QAction shown in a QMenu is triggered by the menu, so
    no caller has to dispatch what it returns. They belong to the widget, not to the
    temporary menu, so they survive it.
*/
    if (actions().isEmpty()) return;
    if (!menu.isEmpty()) menu.addSeparator();
    menu.addActions(actions());
}

void Filters::showSearchQueryMenu(const QPoint &globalPos)
{
/*
    The context menu for the "Enter search query" row: the three things that are about
    the QUERY, then the panel's ordinary filter actions.

    THE FILTER ACTIONS STAY ON IT. Right-clicking anywhere in the tree has always offered
    "Clear all filters", "Expand all" and the rest, and the search row is in the tree.
    Taking them away on the one row a user is most likely to right-click -- because it is
    the only row that is TYPED into -- would make the menu unpredictable. They are
    appended, not replaced, and they run themselves: a QAction shown in a QMenu is
    triggered by the menu, so nothing here has to dispatch them.

    SAVE IS GREYED WITH ITS REASON IN THE ITEM rather than opening a dialog that then
    says there is nothing to save.
*/
    if (G::isLogger) G::log("Filters::showSearchQueryMenu");

    const QString query = currentSearchText();

    QMenu menu(this);

    QAction *larger = menu.addAction(tr("Larger query space..."));

    QAction *save = menu.addAction(query.isEmpty()
                                   ? tr("Save query... (no query to save)")
                                   : tr("Save query..."));
    save->setEnabled(!query.isEmpty());

    QMenu *loadMenu = menu.addMenu(tr("Load query"));
    const QList<QPair<QString, QString>> queries = savedSearchQueries();
    if (queries.isEmpty()) {
        QAction *none = loadMenu->addAction(tr("No saved queries"));
        none->setEnabled(false);
    }
    else {
        for (const QPair<QString, QString> &q : queries) {
            QAction *a = loadMenu->addAction(q.first);
            a->setToolTip(q.second);
            a->setData(q.second);
        }
        loadMenu->setToolTipsVisible(true);
    }

    addFilterActions(menu);

    QAction *chosen = menu.exec(globalPos);
    if (chosen == nullptr) return;

    if (chosen == larger) {
        editSearchInLargeSpace();
        return;
    }
    if (chosen == save) {
        saveSearchQuery();
        return;
    }
    if (chosen->parent() == loadMenu && chosen->data().isValid()) {
        setSearchText(chosen->data().toString());
        return;
    }
    /* Anything else is one of the filter actions, which the menu has already run. */
}

void Filters::editSearchInLargeSpace()
{
/*
    Edit the query in a window instead of in the tree row.

    The row is a single-line editor as wide as a dock panel, which is enough for "heron"
    and not enough for the grammar it accepts: brackets, quoted phrases and negations
    scroll out of sight while they are being typed. This is the same text in a resizable
    box, with the grammar beside it.

    IT COMMITS THE SAME WAY THE ROW DOES -- through setSearchText, so the guarded write,
    the searchStringChange and the filterChange all happen exactly once. The text is
    simplified() on the way in because Return in a QPlainTextEdit is a newline, and the
    row shows one line.
*/
    if (G::isLogger) G::log("Filters::editSearchInLargeSpace");

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Search query"));

    QVBoxLayout *layout = new QVBoxLayout(&dlg);

    QPlainTextEdit *edit = new QPlainTextEdit(currentSearchText(), &dlg);
    edit->setTabChangesFocus(true);
    layout->addWidget(edit, 1);

    QLabel *hint = new QLabel(
        tr("heron nanaimo\t\tboth must appear (AND is the default)\n"
           "heron OR eagle\t\teither may appear\n"
           "(heron OR eagle) tide\tbrackets group\n"
           "\"great blue\"\t\tthe phrase, not the two words\n"
           "-heron\t\t\tmust NOT appear\n\n"
           "OR and NOT are recognised in upper case only."), &dlg);
    layout->addWidget(hint);

    QDialogButtonBox *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(buttons);

    dlg.resize(600, 320);
    edit->setFocus();

    if (dlg.exec() != QDialog::Accepted) return;
    setSearchText(edit->toPlainText().simplified());
}

void Filters::saveSearchQuery()
{
/*
    Name the current query and keep it. The name is offered as the query itself, because
    most queries are short enough to be their own name and the ones that are not are
    exactly the ones worth renaming.
*/
    if (G::isLogger) G::log("Filters::saveSearchQuery");

    const QString query = currentSearchText();
    if (query.isEmpty()) return;

    bool ok = false;
    QString name = QInputDialog::getText(this, tr("Save query"), tr("Name:"),
                                         QLineEdit::Normal, query.left(60), &ok).trimmed();
    if (!ok || name.isEmpty()) return;

    QList<QPair<QString, QString>> queries = savedSearchQueries();

    for (int i = 0; i < queries.count(); ++i) {
        if (queries.at(i).first.compare(name, Qt::CaseInsensitive) != 0) continue;
        const auto answer =
            QMessageBox::question(this, tr("Save query"),
                                  tr("\"%1\" already exists. Replace it?").arg(name));
        if (answer != QMessageBox::Yes) return;
        queries[i].second = query;
        writeSavedSearchQueries(queries);
        return;
    }

    queries << qMakePair(name, query);
    std::sort(queries.begin(), queries.end(),
              [](const QPair<QString, QString> &a, const QPair<QString, QString> &b) {
                  return a.first.compare(b.first, Qt::CaseInsensitive) < 0;
              });
    writeSavedSearchQueries(queries);
}

QList<QPair<QString, QString>> Filters::savedSearchQueries() const
{
    QList<QPair<QString, QString>> queries;
    if (G::settings == nullptr) return queries;

    const int count = G::settings->beginReadArray("SavedSearchQueries");
    for (int i = 0; i < count; ++i) {
        G::settings->setArrayIndex(i);
        const QString name = G::settings->value("name").toString();
        const QString query = G::settings->value("query").toString();
        if (!name.isEmpty() && !query.isEmpty()) queries << qMakePair(name, query);
    }
    G::settings->endArray();
    return queries;
}

void Filters::writeSavedSearchQueries(const QList<QPair<QString, QString>> &queries)
{
/*
    The array is REMOVED before it is written, because beginWriteArray leaves any entries
    beyond the new size in place: without this, deleting or replacing would leave the
    tail of the previous list behind.
*/
    if (G::settings == nullptr) return;

    G::settings->remove("SavedSearchQueries");
    G::settings->beginWriteArray("SavedSearchQueries");
    for (int i = 0; i < queries.count(); ++i) {
        G::settings->setArrayIndex(i);
        G::settings->setValue("name", queries.at(i).first);
        G::settings->setValue("query", queries.at(i).second);
    }
    G::settings->endArray();
}

/* ---------------------------------------------------------------------------------
   The Filter dock's shared-category interface (G::useFilterPanel)
   --------------------------------------------------------------------------------- */

QString Filters::currentSearchText() const
{
    const QString t = searchTrue->text(0);
    return t == enterSearchString ? QString() : t;
}

void Filters::setSearchText(const QString &text)
{
/*
    Drive the Search category from the panel's search box.

    ONE BOX FOR BOTH SCOPES is the point: "here" (Folders) and "everywhere" (Catalog) are
    meant to be the same question asked of different sets, and two separate text fields
    would be two places to type it. The editable searchTrue tree item stays as the
    STORAGE -- the predicate reads its text, and the placeholder is still what "no search"
    means -- but the panel's box is what the user touches.

    The trimmed empty string restores the placeholder rather than storing "", because
    ignoreSearchStrings is what DataModel::searchStringChange tests to decide there is no
    search at all.
*/
    if (G::isLogger) G::log("Filters::setSearchText", text);

/*
    THE WRITE IS GUARDED BECAUSE dataChanged CANNOT TELL IT FROM A USER EDIT.

    setText on the searchTrue item reaches Filters::dataChanged with Qt::EditRole among
    its roles, which is the inline-edit path -- and that path emits searchStringChange,
    and filterChange too when the item is checked. So this function was running the whole
    thing TWICE: two passes of DataModel::searchStringChange over every row (43,070 of
    them in a catalog), and up to two dm->newInstance() bumps, one of which lands on work
    the first had already dispatched. That instance churn is what fills the issue log with
    "Instance clash" and is how a decode in flight ends up discarded.
*/
    const QString t = text.trimmed();
    settingSearchText = true;
    searchTrue->setText(0, t.isEmpty() ? enterSearchString : t);
    settingSearchText = false;
    searchString = t;
    emit searchStringChange(searchString);
    setEachCatTextColor();
    emit filterChange("Filters::setSearchText");
}

bool Filters::loadCatalogCategories()
{
/*
    Fill every dynamic category from the CATALOG rather than the datamodel -- the
    Catalog half of the scope switch.

    THE SAME TREE, THE SAME ITEMS, THE SAME GESTURES. Nothing about how a category item is
    checked, excluded, coloured or counted changes with the scope; only where the values
    came from. That is the whole reason the two docks were merged, and it is why this
    fills the existing categories through addCategoryItems rather than building a parallel
    widget that would drift.

    CATEGORIES THE INDEX CANNOT ANSWER ARE HIDDEN, not shown empty. Duplicates is a
    comparison of what is loaded and has no meaning across a library; the Search category
    is the panel's own box. An empty category reads as "you have no camera models", which
    would be a lie.

    A CATEGORY OF NOTHING BUT BLANK IS ALSO HIDDEN. Catalog::categoryItems now returns the
    blank value as a row -- that is what makes a category add up to the catalog -- so "no
    titles at all" arrives as one item counting every image rather than as an empty map.
    Offering "" as the sole thing to check would filter to everything, so the emptiness
    test asks whether any REAL value came back, not whether the map has entries.

    COUNTS ARE THE CATALOG'S TOTALS and go in BOTH count columns. Per-item counts under
    the live query would be a GROUP BY per category per keystroke over a quarter of a
    million rows; the panel says the counts are library totals rather than showing a
    filtered number that is quietly wrong.
*/
    if (G::isLogger) G::log("Filters::loadCatalogCategories");

    if (!Catalog::instance().isAvailable()) return false;

    categoriesFrom = FromCatalog;

    /* Building the list sets a check state on every row, and each one would otherwise
       emit itemChanged. */
    const QSignalBlocker block(this);

    QMutexLocker locker(&mutex);
    removeChildrenDynamicFilters();
    locker.unlock();

    Catalog &cat = Catalog::instance();
    struct Cat { QTreeWidgetItem *item; int dmColumn; };
    const QVector<Cat> cats {
        {picks,        G::PickColumn},
        {ratings,      G::RatingColumn},
        {labels,       G::LabelColumn},
        {types,        G::TypeColumn},
        {folders,      G::FolderNameColumn},
        {years,        G::YearColumn},
        {months,       G::MonthColumn},
        {days,         G::DayColumn},
        {models,       G::CameraModelColumn},
        {lenses,       G::LensColumn},
        {focalLengths, G::FocalLengthColumn},
        {isos,         G::ISOColumn},
        {titles,       G::TitleColumn},
        {keywords,     G::KeywordsAllColumn},
        {creators,     G::CreatorColumn},
    };

    for (const Cat &c : cats) {
        const QMap<QString, int> map = cat.categoryItems(c.dmColumn);
        addCategoryItems(map, c.item);
        /* Both columns get the library total: column 2 is normally the filtered count,
           and leaving it blank would make every row look half-loaded. */
        for (QTreeWidgetItem *child : itemsInCategory(c.item)) {
            const int n = map.value(itemMapKey(c.item, child), 0);
            child->setData(2, Qt::EditRole, n);
            child->setData(3, Qt::EditRole, n);
        }
        bool anyRealValue = false;
        for (auto it = map.constBegin(); it != map.constEnd(); ++it)
            if (!it.key().isEmpty()) { anyRealValue = true; break; }
        setRowHidden(indexOfTopLevelItem(c.item), QModelIndex(), !anyRealValue);
    }

    /* Duplicates compares what is loaded; Search is the panel's own box. Availability
       is a fact about the rows in the model and about the mount table right now, not
       something the index holds -- BuildFilters counts it from the datamodel like any
       other category, and it shows itself when a row is not Present. */
    setRowHidden(indexOfTopLevelItem(compare), QModelIndex(), true);
    setRowHidden(indexOfTopLevelItem(search), QModelIndex(), true);
    updateAvailabilityVisibility();

    filtersBuilt = true;
    setKeywordCategoryToolTip();
    setEachCatTextColor();
    return true;
}

void Filters::showAllCategories()
{
    if (G::isLogger) G::log("Filters::showAllCategories");
    categoriesFrom = FromDatamodel;
    for (int i = 0; i < topLevelItemCount(); i++)
        setRowHidden(i, QModelIndex(), false);
    /*  SEARCH IS A CATEGORY AGAIN, in both scopes. It was hidden for a while because the
        Filter panel carried a QLineEdit above the tree, which made the query a thing
        beside the filters rather than one of them -- and left the two rows the category
        has always had (matches, and "No match") with nowhere to live. The editable
        searchTrue item is both the storage and the box now, as it was before the catalog
        arrived. See "Searching Folders and Catalog" in Documentation.txt. */
    /* Availability is conditional on what is loaded, so "show all" does not mean it. */
    updateAvailabilityVisibility();
}

void Filters::fillQuery(CatalogQuery &q) const
{
/*
    The checked and excluded items as a catalog query.

    KEYWORDS TAKE THEIR OWN FIELDS because they need a join rather than a column compare;
    everything else goes into the generic maps keyed by datamodel column, so a category
    added to the panel reaches the query without this function growing a case for it.
*/
    struct Cat { QTreeWidgetItem *item; int dmColumn; };
    const QVector<Cat> cats {
        {picks,        G::PickColumn},
        {ratings,      G::RatingColumn},
        {labels,       G::LabelColumn},
        {types,        G::TypeColumn},
        {folders,      G::FolderNameColumn},
        {years,        G::YearColumn},
        {months,       G::MonthColumn},
        {days,         G::DayColumn},
        {models,       G::CameraModelColumn},
        {lenses,       G::LensColumn},
        {focalLengths, G::FocalLengthColumn},
        {isos,         G::ISOColumn},
        {titles,       G::TitleColumn},
        {keywords,     G::KeywordsAllColumn},
        {creators,     G::CreatorColumn},
    };

    for (const Cat &c : cats) {
        QStringList inc, exc;
        /*  itemsInCategory rather than childCount, and the item's VALUE rather than its
            label: the Keywords category is nested and its labels are leaves, so a
            childCount loop would see only the roots and a label would bind the wrong
            one of two same-named keywords. */
        for (QTreeWidgetItem *child : itemsInCategory(c.item)) {
            const Qt::CheckState st = child->checkState(0);
            const QString value = itemMapKey(c.item, child);
            if (st == Qt::Checked) inc << value;
            else if (st == Qt::PartiallyChecked) exc << value;
        }
        if (c.dmColumn == G::KeywordsAllColumn) {
            q.keywords = inc;
            q.excludeKeywords = exc;
        }
        else {
            if (!inc.isEmpty()) q.include.insert(c.dmColumn, inc);
            if (!exc.isEmpty()) q.exclude.insert(c.dmColumn, exc);
        }
    }
}

bool Filters::isAnyCatalogFilter() const
{
    CatalogQuery q;
    fillQuery(q);
    return !q.keywords.isEmpty() || !q.excludeKeywords.isEmpty()
           || !q.include.isEmpty() || !q.exclude.isEmpty();
}

void Filters::save()
/*
    The filters tree (this) is iterated, and every item that is not Unchecked is added to
    the list as an ItemState, which includes the TOP-LEVEL CATEGORY, the item's FILTER
    VALUE and its STATE. Also, the search text is saved in searchTextState.

    KEYED ON THE ROOT ANCESTOR, NOT THE PARENT. Those were the same thing while every
    category was one level deep. The Keywords category is a tree now, so an item's parent
    is another keyword -- saving "Heron" under "Bird" and then looking for it under
    "Keywords" would restore nothing at all, and restore() has no way to notice it found
    no match. See ItemState.

    KEYED ON THE VALUE, NOT THE LABEL, for the same reason fillQuery is: a keyword's
    label is only its leaf, and two keywords can share one.

    The state travels with the item because there are three of them: an exclusion restored
    as an inclusion would invert what the user asked for, silently, on every rebuild.
*/
{
    if (G::isLogger || G::isFlowLogger) G::log("Filters::save");
    // qDebug() << "Filters::save";

    QMutexLocker locker(&mutex);

    itemStates.clear();

    QTreeWidgetItemIterator it(this);
    while (*it) {
        QTreeWidgetItem *item = *it;
        ++it;
        if (!item->parent()) continue;
        if (item->checkState(0) == Qt::Unchecked) continue;

        QTreeWidgetItem *root = item;
        while (root->parent()) root = root->parent();

        ItemState state;
        state.category = root->text(0);
        state.value = itemMapKey(root, item);
        state.state = item->checkState(0);
        itemStates << state;
    }
    searchText = searchTrue->text(0);
    if (searchText == enterSearchString) searchText = "";
}

void Filters::restore()
/*
    First, uncheck all items to start with a clean state.  Then iterate through all
    the ItemState and checking the matching item in the QWidgetTree (this).
*/
{
    if (G::isLogger || G::isFlowLogger) G::log("Filters::restore");
    uncheckAllFilters();

    /*  Keyed the same way save() keys, which is the whole point: {top-level category,
        filter value}. Built once rather than searched per state. */
    QHash<QString, QHash<QString, QTreeWidgetItem *>> byCategoryAndValue;
    QTreeWidgetItemIterator it(this);
    while (*it) {
        QTreeWidgetItem *item = *it;
        ++it;
        if (!item->parent()) continue;
        QTreeWidgetItem *root = item;
        while (root->parent()) root = root->parent();
        byCategoryAndValue[root->text(0)].insert(itemMapKey(root, item), item);
    }

    // restore checked items
    for (const auto &state : itemStates) {
        QTreeWidgetItem *item =
            byCategoryAndValue.value(state.category).value(state.value, nullptr);
        /*  A state with no matching item is normal rather than an error: the value is
            simply not in this folder, or the keyword was re-parented and its path
            changed, in which case dropping the filter is correct. */
        if (!item) continue;
        item->setCheckState(0, state.state);
        styleFilterItem(item);
    }
    searchTrue->setText(0, searchText);
    // emit filterChange("Filters::restore");
}

void Filters::reportSaved()
{
    for (const auto& state : itemStates) {
        qDebug().noquote() << state.category.leftJustified(15)
                           << state.value;
    }

}

void Filters::checkItem(QTreeWidgetItem *par, QString itemName, Qt::CheckState state)
{
/*
    Iterates the category to find the item and sets the item checkbox to state (true or false)
*/
    if (G::isLogger) G::log("Filters::checkItem");
    if (debugFilters)
        qDebug() << "Filters::checkItem"
                 << "parent =" << par->text(0)
                 << "itemName =" << itemName
                 << "state =" << state
                    ;
    for (int i = 0; i < par->childCount(); i++) {
        QTreeWidgetItem *item = par->child(i);
        if (item->data(0, Qt::DisplayRole).toString() == itemName) {
            item->setCheckState(0, state);
            styleFilterItem(item);
            emit filterChange("Filters::checkItem");
        }
    }
}

void Filters::uncheckAllFilters()
{
/*
    Uncheck all the filter items
*/
    if (G::isLogger) G::log("Filters::uncheckAllFilters");
    if (debugFilters)
        qDebug() << "Filters::uncheckAllFilters"
                    ;
    QMutexLocker locker(&mutex);

    /* Nothing is checked any more, so there is nothing to range from. */
    rangeAnchorCategory = nullptr;
    rangeAnchorItem.clear();

    QTreeWidgetItemIterator it(this);
    while (*it) {
        if ((*it)->parent()) {
            (*it)->setCheckState(0, Qt::Unchecked);
            /* Clears the strikethrough an exclusion left behind: unchecking the box is
               not enough, because exclusion is drawn on the FONT as well. */
            styleFilterItem(*it);
            (*it)->setData(2, Qt::EditRole, "");
            (*it)->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
            (*it)->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
        }
        else {
            (*it)->setForeground(0, QBrush(G::textColor));
        }
        ++it;
    }
    setSearchNewFolder();
}

//void Filters::uncheckTypesFilters()  // not used
//{
///*
//    Uncheck types.  This is required when raw + jpg are either combined or not combined.
//*/
//    if (G::isLogger) G::log("Filters::uncheckTypesFilters");
//    if (debugFilters)
//        qDebug() << "Filters::uncheckTypesFilters"
//                    ;
//    QTreeWidgetItemIterator it(this);
//    while (*it) {
//        if ((*it)->parent() == types) {
//            (*it)->setCheckState(0, Qt::Unchecked);
//            (*it)->setData(2, Qt::EditRole, "");
//            (*it)->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
//            (*it)->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
//        }
//        ++it;
//    }
//}

void Filters::expandAllFilters()
{
    if (G::isLogger) G::log("Filters::expandAllFilters");
    if (debugFilters)
        qDebug() << "Filters::expandAllFilters"
                    ;
    expandAll();
}

void Filters::collapseAllFilters()
{
    if (G::isLogger) G::log("Filters::collapseAllFilters");
    if (debugFilters)
        qDebug() << "Filters::collapseAllFilters"
                    ;
    collapseAll();
}

void Filters::collapseAllFiltersExceptSearch()
{
    if (G::isLogger) G::log("Filters::collapseAllFiltersExceptSearch");
    collapse(indexFromItem(picks));
    collapse(indexFromItem(ratings));
    collapse(indexFromItem(labels));
    collapse(indexFromItem(types));
    collapse(indexFromItem(folders));
    collapse(indexFromItem(years));
    collapse(indexFromItem(months));
    collapse(indexFromItem(days));
    collapse(indexFromItem(models));
    collapse(indexFromItem(lenses));
    collapse(indexFromItem(focalLengths));
    collapse(indexFromItem(isos));
    collapse(indexFromItem(titles));
    collapse(indexFromItem(keywords));
    collapse(indexFromItem(creators));
    collapse(indexFromItem(availability));
    // collapse(indexFromItem(missingThumbs));
    collapse(indexFromItem(compare));
}

void Filters::toggleExpansion()
{
    if (G::isLogger) G::log("Filters::toggleExpansion");
    if (debugFilters)
        qDebug() << "Filters::toggleExpansion"
                    ;
    bool isExpanded = false;

    QMutexLocker locker(&mutex);

    QTreeWidgetItemIterator it(this);
    while (*it) {
        if (!(*it)->parent()) {
            if ((*it)->isExpanded()) {
                isExpanded = true;
                break;
            }
        }
        ++it;
    }
    if (isExpanded) collapseAll();
    else expandAll();
//    qDebug() << "Filters::toggleExpansion" << "isExpanded =" << isExpanded;
}

//void Filters::updateCategoryItems(QStringList itemList, QTreeWidgetItem *category)
//{
///*
//    All the unique values for a category are collected into a QList object in
//    BuildFilters. The list is passed here, where unique values are extracted and added to
//    the category. For example, there could be multiple file types in the folder like JPG
//    and NEF. A QMap object is used so the items can be sorted by key in the same order as
//    the tableView. This function should only be used for dynamic categories - see
//    createDynamicFilters;

//    If a category item was just checked (cjf) then it is ignored, as the user may want
//    to check another item in the same category.
//*/
//    if (G::isLogger || G::isFlowLogger) G::log("Filters::updateCategoryItems", category->text(0));
//    if (debugFilters)
//        qDebug() << "Filters::updateCategoryItems"
//                 << "category =" << category->text(0)
//                    ;
//    if (catItemJustClicked == category) return;
//    if (itemList.size() < 2) return;
////    itemList.sort();

//    // iterate existing category items in filters
//    if (category->childCount()) {
//        for (int i = category->childCount() - 1; i >= 0 ; i--) {
//            QString s = category->child(i)->text(0);
//            // remove from unique itemList
//            if (itemList.contains(s)) {
//                itemList.remove(itemList.indexOf(s));
//            }
//            // remove from filter tree unless checked item
//            else {
//                if (category->child(i)->checkState(0) == Qt::Unchecked) {
//                    category->removeChild(category->child(i));
//                }
//            }
//        }
//    }

//    // add all remaining items in unique itemList to filter tree
//    QTreeWidgetItem *item;
//    for (int i = 0; i < itemList.count(); i++) {
//        QString s = itemList.at(i);
//        item = new QTreeWidgetItem(category);
//        item->setText(0, s);
//        item->setCheckState(0, Qt::Unchecked);
//        item->setData(1, Qt::EditRole, s);
//    }

//    // sort the result
//    category->sortChildren(0, Qt::AscendingOrder);
//}

void Filters::updateSearchCategoryCount(QMap<QString, int> itemMap, bool isFiltered)
{
/*
    Updates search category unfiltered counts.  The search items, "Search text" and
    "No match" are predefined and not dynamically added by BuildFilters::appendUniqueItems.
*/
    int col;
    if (isFiltered) col = 2;
    else col = 3;

    if (G::isLogger) G::log("Filters::updateSearchCategoryCount");

    if (itemMap.contains("true"))
        searchTrue->setData(col, Qt::EditRole, itemMap["true"]);
    else
        searchTrue->setData(col, Qt::EditRole, 0);

    if (itemMap.contains("false"))
        searchFalse->setData(col, Qt::EditRole, itemMap["false"]);
    else
        searchFalse->setData(col, Qt::EditRole, 0);
}

void Filters::updateKeywordItems(const QMap<QString, int> &pathCounts,
                                 QTreeWidgetItem *category)
{
/*
    Refresh the Keywords tree after a keyword edit: rebuild it and put the user's state
    back.

    REBUILT RATHER THAN PATCHED IN PLACE, which is the opposite of what
    updateCategoryItems does for the flat categories. Patching a tree means removing a
    node whose children may still be wanted, re-parenting what is left, and pruning
    branches that have become empty -- three chances to leave the tree malformed, on a
    path that runs after every keyword edit. The vocabulary is a few thousand nodes, not a
    few hundred thousand rows, so rebuilding it is cheap and cannot go half-done.

    WHAT MUST SURVIVE THE REBUILD is what the user set: the check state, and the EXPANSION
    state. Losing the second would collapse the whole tree every time a keyword was added
    to an image, which is the kind of thing that makes a panel feel broken even though
    nothing is wrong. Both are keyed on the PATH, so a node that still exists gets its
    state back and a node that has genuinely gone does not.
*/
    if (G::isLogger) G::log("Filters::updateKeywordItems");

    QHash<QString, Qt::CheckState> stateByPath;
    QHash<QString, bool> expandedByPath;
    for (QTreeWidgetItem *item : itemsInCategory(category)) {
        const QString path = item->data(1, Qt::EditRole).toString();
        if (path.isEmpty()) continue;
        if (item->checkState(0) != Qt::Unchecked)
            stateByPath.insert(keywordFold(path), item->checkState(0));
        if (item->isExpanded()) expandedByPath.insert(keywordFold(path), true);
    }

    {
        QMutexLocker locker(&mutex);
        qDeleteAll(category->takeChildren());
    }

    addKeywordItems(pathCounts, category);

    for (QTreeWidgetItem *item : itemsInCategory(category)) {
        const QString fold = keywordFold(item->data(1, Qt::EditRole).toString());
        const auto it = stateByPath.constFind(fold);
        if (it != stateByPath.constEnd()) {
            item->setCheckState(0, it.value());
            styleFilterItem(item);
        }
        if (expandedByPath.value(fold, false)) item->setExpanded(true);
    }
}

void Filters::updateCategoryItems(QMap<QString, int> itemMap, QTreeWidgetItem *category)
{
/*
    If an item is edited in the DataModel then the list of unique category items may need
    to be updated.  For example, the existing dataset may not have any items with a rating
    of 4.  If the user sets a rating to 4 then this will have to be added to the filter
    list.  On the other hand, if the user changes the only item with a rating of 4 to
    another rating, then the filter list should remove rating 4.

    itemMap includes the count of all unique unfiltered items in the DataModel.  The
    existing filter items are compared to the itemMap.  If a filter item is in the itemMap
    then its count is updated and the item is removed from the itemMap.  If the filter
    item is not in the itemMap then it is removed.  After all filter items have been
    iterated, any remaining items in itemMap are added to the filter items.
*/
    /*  Keywords are a TREE, and this function's remove-then-append pass is one level
        deep in both halves. Delegated for the same reason addCategoryItems is. */
    if (category == keywords) {
        updateKeywordItems(itemMap, category);
        return;
    }

    if (G::isLogger) G::log("Filters::updateCategoryItems", category->text(0));
    if (debugFilters)
        qDebug() << "Filters::updateCategoryItems"
                 << "category =" << category->text(0)
                    ;

    /* The state of an item that is disappearing (a title or creator that was edited), so
       a rename carries its filter across rather than silently cancelling it. The STATE,
       not just "was it checked": restoring an exclusion as an inclusion would reverse
       what the user asked for. */
    Qt::CheckState oldItemState = Qt::Unchecked;

    // remove existing category items in filters if no longer in itemMap
    if (category->childCount()) {
        for (int i = category->childCount() - 1; i >= 0 ; i--) {
            if (G::stop) return;
            QString s = category->child(i)->text(0);
            // count and remove from itemMap
            if (itemMap.contains(s)) {
                // update unfiltered item count
                category->child(i)->setData(3, Qt::EditRole, itemMap[s]);
                itemMap.remove(s);
            }
            // remove from filter tree unless checked item
            else {
                if (category->child(i)->checkState(0) != Qt::Unchecked)
                    oldItemState = category->child(i)->checkState(0);
                category->removeChild(category->child(i));
//                break;
            }
        }
    }

    // add all remaining items in unique itemList to filter tree
    QTreeWidgetItem *item;
    QMapIterator<QString, int> i(itemMap);
    while (i.hasNext()) {
        if (G::stop) return;
        i.next();
        item = new QTreeWidgetItem(category);
        item->setText(0, i.key());
        item->setCheckState(0, oldItemState);
        styleFilterItem(item);
        item->setData(1, Qt::EditRole, filterValueFor(category, i.key()));
        item->setData(3, Qt::EditRole, i.value());
        item->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        item->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
    }

    // sort the result
    category->sortChildren(0, Qt::AscendingOrder);

    /*  This pass REMOVES items as well as adding them, so a scope that has just lost its
        last offline row hides the category again. */
    if (category == availability) updateAvailabilityVisibility();
}

QList<QTreeWidgetItem *> Filters::itemsInCategory(QTreeWidgetItem *category) const
{
/*
    Every filterable item beneath a category, at any depth, in tree order.

    ONE LEVEL WAS AN ASSUMPTION, NOT A FACT, and it was written into a dozen loops as
    "for i < category->childCount()". The Keywords category is nested now, so each of
    those loops would have seen only the roots -- counts on branches, nothing on leaves,
    and no error anywhere. This is the one place that knows how deep a category goes.
*/
    QList<QTreeWidgetItem *> out;
    if (!category) return out;

    std::function<void(QTreeWidgetItem *)> walk = [&](QTreeWidgetItem *parent) {
        for (int i = 0; i < parent->childCount(); ++i) {
            QTreeWidgetItem *child = parent->child(i);
            out << child;
            if (child->childCount()) walk(child);
        }
    };
    walk(category);
    return out;
}

QString Filters::itemMapKey(const QTreeWidgetItem *category,
                            const QTreeWidgetItem *item) const
{
    if (!item) return QString();
    /*  Keywords are keyed on the PATH the item carries, because the label is only the
        leaf and two keywords can share one -- keying on the label would put both
        Vancouvers' counts on whichever item was visited first. */
    if (category == keywords) return item->data(1, Qt::EditRole).toString();
    return item->text(0);
}

void Filters::addKeywordItems(const QMap<QString, int> &pathCounts,
                              QTreeWidgetItem *category)
{
/*
    Build the Keywords category as a TREE, from a map keyed on full paths.

    NO SUMMING, AND THAT IS THE POINT. Every image is linked to every ANCESTOR PREFIX of
    every keyword path it carries (see G::KeywordsAllColumn), so countKeywords has
    already produced a count for "Fauna" as well as for "Fauna|Bird|Heron". Each node
    therefore reads its own count straight out of the map: no second pass, no rolling up
    children, and BuildFilters did not have to change at all. It is the nicest consequence
    of expanding at write time rather than at query time.

    THE MAP'S ORDER PUTS PARENTS FIRST for free: a QMap is sorted by key, and a path is a
    prefix of its children, so "Fauna" is visited before "Fauna|Bird". ensureNode does not
    rely on that -- it creates a missing ancestor on demand -- but it means the common
    case creates nothing on demand at all.

    A SYNTHESISED ANCESTOR gets a zero count rather than being skipped. It should never
    happen (an ancestor with no count would mean an image linked to a leaf but not to its
    prefix, which the expansion makes impossible), but a tree with a hole in it would be
    worse than a branch reading zero.
*/
    QMutexLocker locker(&mutex);

    /*  Seed from whatever is already there, so this composes with a second call the way
        addCategoryItems' duplicate-elimination pass does rather than building the tree
        twice. */
    QHash<QString, QTreeWidgetItem *> byPath;
    for (QTreeWidgetItem *existing : itemsInCategory(category)) {
        const QString path = existing->data(1, Qt::EditRole).toString();
        if (!path.isEmpty()) byPath.insert(keywordFold(path), existing);
    }

    std::function<QTreeWidgetItem *(const QString &)> ensureNode =
        [&](const QString &path) -> QTreeWidgetItem * {
        const QString fold = keywordFold(path);
        if (QTreeWidgetItem *have = byPath.value(fold, nullptr)) return have;

        const QString parentPath = keywordParentPath(path);
        QTreeWidgetItem *parent =
            parentPath.isEmpty() ? category : ensureNode(parentPath);

        QTreeWidgetItem *node = new QTreeWidgetItem(parent);
        /*  THE LABEL IS THE LEAF AND THE VALUE IS THE PATH. A tree that repeated the
            whole path on every row would be unreadable at depth, and a filter that bound
            the leaf would match the wrong Vancouver. */
        node->setText(0, keywordLeafOf(path));
        node->setData(1, Qt::EditRole, path);
        node->setCheckState(0, Qt::Unchecked);
        node->setData(2, Qt::EditRole, 0);
        node->setData(3, Qt::EditRole, 0);
        node->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        node->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
        node->setToolTip(0, path == node->text(0)
            ? QString("Click to include. Opt+click to exclude. Right-click for both.")
            : QString("%1\n\nClick to include. Opt+click to exclude.").arg(path));
        byPath.insert(fold, node);
        return node;
    };

    for (auto it = pathCounts.constBegin(); it != pathCounts.constEnd(); ++it) {
        if (G::stop) return;
        if (it.key().isEmpty()) continue;       // a blank path is a bad row, not a value
        QTreeWidgetItem *node = ensureNode(it.key());
        node->setData(2, Qt::EditRole, it.value());
        node->setData(3, Qt::EditRole, it.value());
    }

    /*
        MARK A TOP-LEVEL KEYWORD THAT SHARES ITS NAME WITH ONE INSIDE THE TREE.

        The tree can hold "Squirrel" at the top AND "Fauna|Animal|Squirrel" beneath, and
        both draw as the single word "Squirrel". They are DIFFERENT keywords -- the whole
        path is the identity -- so checking one and expecting the other's images is a
        filter that appears to do nothing, with nothing on screen to explain it. That is
        what it did.

        Most of these were manufactured by keywordEffectivePaths keeping an ancestor name
        out of dc:subject, which it no longer does. The rest are real: a library where
        some images were tagged flat and others hierarchically genuinely has both, and no
        rule can merge them without deciding for the user that they mean the same thing.
        So the DUPLICATE IS SHOWN AS ONE, in italic and with a tooltip that names the
        other, rather than being hidden or silently folded in.

        ONLY THE TOP-LEVEL COPY IS MARKED. The one inside the tree is not ambiguous: its
        parents are on screen above it, and its tooltip already carries the full path.
    */
    QHash<QString, QStringList> deeperByName;   // folded leaf -> paths of depth > 1
    for (QTreeWidgetItem *item : itemsInCategory(category)) {
        const QString path = item->data(1, Qt::EditRole).toString();
        if (!path.contains('|')) continue;
        deeperByName[keywordFold(keywordLeafOf(path))] << path;
    }
    for (int i = 0; i < category->childCount(); ++i) {
        QTreeWidgetItem *item = category->child(i);
        const QString path = item->data(1, Qt::EditRole).toString();
        if (path.isEmpty() || path.contains('|')) continue;
        const QStringList also = deeperByName.value(keywordFold(path));
        if (also.isEmpty()) continue;
        QFont f = item->font(0);
        f.setItalic(true);
        item->setFont(0, f);
        item->setToolTip(0, QString(
            "%1\n\nA TOP-LEVEL keyword, filed under nothing. The same name also appears "
            "in the tree as:\n    %2\n\nThose are different keywords -- a keyword is its "
            "whole path -- so this filters only the images tagged with the bare name.\n\n"
            "Click to include. Opt+click to exclude.")
            .arg(path, also.join("\n    ")));
    }
}

void Filters::addCategoryItems(QMap<QString, int> itemMap, QTreeWidgetItem *category)
{
/*
    All the unique values for a category are collected into a QMap object in
    BuildFilters. The list is passed here, where unique values are extracted and added to
    the category. For example, there could be multiple file types in the folder like JPG
    and NEF. A QMap object is used so the items can be sorted by key in the same order as
    the tableView. This function should only be used for dynamic categories - see
    createDynamicFilters.

    The itemMap contains the total unfiltered count for the item.  This is set for both
    unfiltered and filtered totals since the DataModel has not been filtered at this time.
*/
//    if (G::isLogger || G::isFlowLogger)
//        G::log("Filters::addCategoryItems", category->text(0));
    if (debugFilters)
        qDebug() << "Filters::addCategoryItems"
                 << "category =" << category->text(0)
                    ;

    /*  Keywords are a TREE and have their own builder. Delegating here rather than at
        every call site keeps the BuildFilters op dispatch and the catalog-scope loop
        generic, which is what stops one of them being forgotten. */
    if (category == keywords) {
        addKeywordItems(itemMap, category);
        return;
    }

    QMutexLocker locker(&mutex);

    // eliminate any items in itemMap that are already items in the category to avoid
    // duplicates
    for (int i = 0; i < category->childCount(); ++i) {
        if (G::stop) return;
        QTreeWidgetItem* child = category->child(i);
        QString text = child->text(0);
        if (itemMap.contains(text)) {
            itemMap.remove(text);
        }
    }

    /*  ITEM ORDER IS THE MAP'S KEY ORDER, which is alphabetical -- right for a name and
        right for the values deliberately padded so that they sort as numbers (see the
        focal length and ISO justification in BuildFilters::makeSnapshot), but wrong for
        a closed vocabulary whose meaning is a SEQUENCE. Months sorted as text read Apr,
        Aug, Dec, Feb ..., which is not a list of months; they are inserted in calendar
        order instead, and only the months present are offered. */
    QStringList keys = itemMap.keys();
    if (category == months) {
        QStringList inCalendarOrder;
        for (const QString &name : Catalog::monthLabels())
            if (itemMap.contains(name)) inCalendarOrder << name;
        /*  A row with no capture date counts as the blank item, exactly as it does
            under Years and Days, and it is not a month so it goes last. */
        if (itemMap.contains("")) inCalendarOrder << "";
        keys = inCalendarOrder;
    }

    // add all remaining items in unique itemList to filter tree
    QTreeWidgetItem *item;
    for (const QString &key : keys) {
        const int count = itemMap.value(key);
        item = new QTreeWidgetItem(category);
        item->setText(0, key);
        item->setCheckState(0, Qt::Unchecked);
        item->setData(1, Qt::EditRole, filterValueFor(category, key));
        item->setData(2, Qt::EditRole, count);
        item->setData(3, Qt::EditRole, count);
        item->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        item->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
    }

    // sort the result
//    category->sortChildren(0, Qt::AscendingOrder);

    /*  Unlocked first: this touches the tree's row visibility, not the item lists the
        mutex guards, and updateAvailabilityVisibility does not take it. */
    locker.unlock();
    if (category == availability) updateAvailabilityVisibility();
}

QVariant Filters::filterValueFor(const QTreeWidgetItem *category, const QString &label) const
{
/*
    See the declaration. One category, one exception, stated here rather than at the
    two sites that build items.
*/
    if (category == availability) return Catalog::availabilityCode(label);
    return label;
}

void Filters::updateAvailabilityVisibility()
{
/*
    SHOWN ALWAYS, DISABLED WHEN THERE IS NOTHING TO CHOOSE. It used to be hidden while
    every loaded row was Present, and hiding it answered a question the user never got to
    ask: "can Winnow tell me which of these files it cannot open?" A category that appears
    and disappears with the data is also a category nobody learns is there. Greyed out
    with its items still visible says the same thing -- nothing here is offline, missing
    or unreadable -- and says it where the user is looking.

    "Any value that is not Present" rather than "more than one item", because a catalog
    scope in which EVERY row is offline is exactly the case worth enabling it for.
*/
    bool anyNotPresent = false;
    const QString present = Catalog::availabilityLabel(int(Catalog::Availability::Present));
    for (int i = 0; i < availability->childCount(); ++i) {
        if (availability->child(i)->text(0) != present) { anyNotPresent = true; break; }
    }
    setRowHidden(indexOfTopLevelItem(availability), QModelIndex(), false);
    availability->setDisabled(!anyNotPresent);
}

void Filters::updateUnfilteredCountPerItem(QMap<QString, int> itemMap, QTreeWidgetItem *category)
{
    /*
    All the unique values for a category are collected into a QMap object in
    BuildFilters. The list is passed here, where unique values are extracted and
    added to the category. For example, there could be multiple file types in the
    folder like JPG and NEF. A QMap object is used so the items can be sorted by key
    in the same order as the tableView. This function should only be used for
    dynamic categories - see createDynamicFilters;

    If a category item was just checked (activeCategory) then it is ignored, as the
    user may want to check another item in the same category.
*/
    //    if (G::isLogger || G::isFlowLogger) G::log("Filters::addFilteredCountPerItem", category->text(0));
    if (debugFilters)
        qDebug() << "Filters::updateUnfilteredCountPerItem"
                 << "category =" << category->text(0)
                 << "itemMap =" << itemMap
            ;

    QMutexLocker locker(&mutex);

    for (QTreeWidgetItem *item : itemsInCategory(category)) {
        item->setData(3, Qt::EditRole, 0);
        const QString key = itemMapKey(category, item);
        if (itemMap.contains(key))
            item->setData(3, Qt::EditRole, itemMap.value(key));
    }

    // sort the result
    //category->sortChildren(0, Qt::AscendingOrder);
}

void Filters::updateFilteredCountPerItem(QMap<QString, int> itemMap, QTreeWidgetItem *category)
{
/*
    All the unique values for a category are collected into a QMap object in
    BuildFilters. The list is passed here, where unique values are extracted and
    added to the category. For example, there could be multiple file types in the
    folder like JPG and NEF. A QMap object is used so the items can be sorted by key
    in the same order as the tableView. This function should only be used for
    dynamic categories - see createDynamicFilters;

    If a category item was just checked (activeCategory) then it is ignored, as the
    user may want to check another item in the same category.
*/
    // if (G::isLogger || G::isFlowLogger) G::log("Filters::addFilteredCountPerItem", category->text(0));
    if (debugFilters)
        qDebug() << "Filters::updateFilteredCountPerItem"
                 << "category =" << category->text(0)
                    ;

    QMutexLocker locker(&mutex);

    for (QTreeWidgetItem *item : itemsInCategory(category)) {
        if (G::stop) return;
        item->setData(2, Qt::EditRole, 0);
        const QString key = itemMapKey(category, item);
        if (itemMap.contains(key))
            item->setData(2, Qt::EditRole, itemMap.value(key));
    }

    // sort the result
    //category->sortChildren(0, Qt::AscendingOrder);
}

void Filters::updateZeroCountCheckedItems(QMap<QString, int> itemMap, QTreeWidgetItem *category)
{
    /*
    All the unique values for a category are collected into a QMap object in
    BuildFilters. The list is passed here, where category child items that are
    checked and have a zero unfiltered count are unchecked.

    For example, if the Pick category picked items is checked, and all the datamodel
    picked rows have been unpicked, then the filter picked item will be checked but
    have a zero filtered count. The filter picked item must be unchecked to avoid a
    proxy filter null result.
*/
    //if (debugFilters || G::isLogger || G::isFlowLogger)
        qDebug() << "Filters::updateZeroCountCheckedItems"
                 << "category =" << category->text(0)
            ;

    QMutexLocker locker(&mutex);

    for (QTreeWidgetItem *child : itemsInCategory(category)) {
        // is the item checked
        qDebug() << "Filters::updateZeroCountCheckedItems prior"
                 << "item =" << child->text(0)
                 << "checkState =" << child->checkState(0);
        /* Qt::Checked only. An EXCLUSION of something with a zero count excludes
           nothing, so it cannot produce the null result this guards against -- and
           clearing it would silently discard the user's intent the moment the value
           came back into the folder. */
        if (child->checkState(0) == Qt::Checked) {
            const QString key = itemMapKey(category, child);
            // if the unfiltered item count zero then uncheck item
            if (itemMap.contains(key) && itemMap.value(key) == 0) {
                child->setCheckState(0, Qt::Unchecked);
                qDebug() << "Filters::updateZeroCountCheckedItems after"
                         << "item =" << child->text(0)
                         << "checkState =" << child->checkState(0);
            }
        }
    }

    // sort the result
    //category->sortChildren(0, Qt::AscendingOrder);
}

void Filters::dataChanged(const QModelIndex &topLeft,
                          const QModelIndex &bottomRight,
                          const QVector<int> &roles)
{
/*
    If the user clicks on the checkbox indicator of any child item then the checkbox state
    toggles and dataChanged is triggered. The dataChanged function sets the
    itemCheckStateHasChanged flag to true. Next the itemClickedSignal is fired. Since the
    itemCheckStateHasChanged flag is true the function itemClickedSignal only emits a
    filterChange.

    If the user clicks on the text portion of the checkbox (ie "Purple" in the color class
    filters) then the checkbox is not toggled and the dataChanged is not triggered. The
    itemClickedSignal is fired and since the itemCheckStateHasChanged flag is false the
    checkbox checkstate is manually toggled and a filterChange is emitted.

    If the user clicks on the text portion of the search checkbox then the itemClickedSignal
    is fired and the itemClickedSignal function detects that the item is searchText and
    itemCheckStateHasChanged is false and sets the searchText cell to edit mode. The user
    makes an edit. This fires the itemChangedSignal. The dataChanged function knows the sender
    is the item searchText and the role is Qt::EditMode. The searchString is updated to the
    new value and searchStringChange is emitted. DataModel::searchStringChange receives the
    signal and updates the datamodel searchColumn match to true or false for each row. The
    filteredItemCount is updated.
*/
    //if (G::isLogger) G::log("Filters::dataChanged");
    //qDebug() << "Filters::dataChanged" << topLeft << bottomRight;

    // checkstate has changed
    if (roles.contains(Qt::CheckStateRole)) {
        itemCheckStateHasChanged = true;
        QTreeWidget::dataChanged(topLeft, bottomRight, roles);
        return;
    }

    itemCheckStateHasChanged = false;

    /*  A programmatic write from setSearchText, which emits both signals itself. Without
        this the panel's search box ran the whole pass twice -- see setSearchText. */
    if (settingSearchText) {
        QTreeWidget::dataChanged(topLeft, bottomRight, roles);
        return;
    }

    // searchText has changed
    if (roles.contains(Qt::EditRole)) {
        if (topLeft.column() != 0) return;
        QTreeWidgetItem *item = itemFromIndex(topLeft);
        if (item == searchTrue) {
            // qDebug() << "Search string change";
            // no search string
            if (searchTrue->text(0) == "") {
                searchString = "";
                searchTrue->setText(0, enterSearchString);
                emit searchStringChange(searchString);
                if (item->checkState(0) == Qt::Unchecked) return;
                emit filterChange("Filters::itemChangedSignal search text change");
                return;
            }
            /*  NOT LOWER-CASED. It used to be, from when a search was a substring test
                and case did not matter. It matters now: the grammar reads OR, AND and
                NOT as operators only in capitals (SearchTerms::parse), so lowering the
                text turned "heron OR eagle" into a search for the word "or". Term
                matching is case-insensitive inside the parser. */
            searchString = searchTrue->text(0).trimmed();
            emit searchStringChange(searchString);
            if (isCatFiltering(search)) search->setForeground(0, QBrush(hdrIsFilteringColor));
            else search->setForeground(0, QBrush(G::textColor));
            if (item->checkState(0) == Qt::Unchecked) return;
            emit filterChange("Filters::itemChangedSignal search text change");
            return;
        }
    }

    QTreeWidget::dataChanged(topLeft, bottomRight, roles);

}

void Filters::itemClickedSignal(QTreeWidgetItem *item, int column)
{
/*
    If the user clicks on the checkbox indicator of any child item then the checkbox state
    toggles and dataChanged is triggered. The dataChanged function sets the
    itemCheckStateHasChanged flag to true. Next the itemClickedSignal is fired. Since the
    itemCheckStateHasChanged flag is true the function itemClickedSignal only emits a
    filterChange.

    If the user clicks on the text portion of the checkbox (ie "Purple" in the color class
    filters) then the checkbox is not toggled and the dataChanged is not triggered. The
    itemClickedSignal is fired and since the itemCheckStateHasChanged flag is false the
    checkbox checkstate is manually toggled and a filterChange is emitted.

    If the user clicks on the text portion of the search checkbox then the itemClickedSignal
    is fired and the itemClickedSignal function detects that the item is searchText and
    itemCheckStateHasChanged is false and sets the searchText cell to edit mode. The user
    makes an edit. This fires the itemChangedSignal. The dataChanged function knows the sender
    is the item searchText and the role is Qt::EditMode. The searchString is updated to the
    new value and searchStringChange is emitted. DataModel::searchStringChange receives the
    signal and updates the datamodel searchColumn match to true or false for each row. The
    filteredItemCount is updated.
*/
    if (G::isLogger) G::log("Filters::itemClickedSignal");
    if (debugFilters)
        qDebug() << "Filters::itemClickedSignal"
                 << "column =" << column
                 << "item =" << item->text(0)
                 << "parent =" << item->parent()->text(0)
                 << "itemCheckStateHasChanged" << itemCheckStateHasChanged
                 << "G::allMetadataAttempted =" << G::allMetadataAttempted
                    ;
    /* The datamodel-readiness guards apply only when the items DESCRIBE the datamodel. A
       catalog category item is answerable whether or not the loaded folder has finished
       reading its metadata -- indeed whether or not a folder is loaded at all -- and
       applying them there swallowed the click silently: the checkbox toggled (QTreeWidget
       does that itself, before this runs) but filterChange was never emitted, so the Find
       dock never re-ran its query and Load stayed disabled. */
    const bool needsModel = categoriesFrom == FromDatamodel;

    // Only interested in clicks on column 0 (checkbox + text)
    if (item->isDisabled() ||
        column > 0 ||
        !item->parent() ||
        /*  A LOAD RUNNING, not "every row read" -- see the note in MW::filterChange.
            G::allMetadataAttempted goes false whenever the scroll-in verifier clears a
            stale row, which silently disabled every filter click. */
        (needsModel && G::isModifyingDatamodel) ||
        (needsModel && buildingFilters))
    {
        /*
        qDebug() << "Filters::itemClickedSignal failed"
                 << "G::allMetadataAttempted =" << G::allMetadataAttempted
                 << "G::iconChunkLoaded =" << G::iconChunkLoaded
                 << "buildingFilters =" << buildingFilters
            ; //*/
        return;
    }

    if (!itemCheckStateHasChanged) {
        /*
        qDebug() << "Filters::itemClickedSignal"
                 << "item->parent() =" << item->parent()->text(0)
                 << "item =" << item->text(0)
                 << "itemCheckStateHasChanged =" << itemCheckStateHasChanged;
        //          */
        // clicked on the search text then edit it - this triggers Filters::dataChanged
        if (item == searchTrue) {
            editItem(searchTrue, 0);
            return;
        }
        // clicked on checkbox text (not the indicator) so toggle the check state
        else {
            /* Two states here, not three: an excluded item is intercepted in
               mousePressEvent and never reaches this. Off becomes included, anything
               else becomes off. */
            itemCheckStateHasChanged = false;
            if (item->checkState(0) == Qt::Unchecked) item->setCheckState(0, Qt::Checked);
            else item->setCheckState(0, Qt::Unchecked);
            styleFilterItem(item);
        }
    }

    /* Whether the click landed on the indicator (Qt toggled it) or on the text (we did),
       an item that ends up checked is where the next Shift+click ranges from. */
    if (item->checkState(0) == Qt::Checked) noteRangeAnchor(item);

    activeCategory = item->parent();
    emit filterChange("Filters::itemClickedSignal");
}

void Filters::setSoloMode(bool isSolo)
{
    if (G::isLogger) G::log("Filters::setSoloMode");
    this->isSolo = isSolo;
}

void Filters::resizeColumns()
{
    if (G::isLogger) G::log("Filters::resizeColumns");
    hideColumn(1);
    QFont font = this->font();
//    font.setPointSize(G::fontSize.toInt());
    QFontMetrics fm(font);
//    int decorationWidth = 25;       // the expand/collapse arrows
    int countColumnWidth = fm.boundingRect("-99999-").width();
    int countFilteredColumnWidth = fm.boundingRect("-99999-").width();
    int col0Width = viewport()->width() - countColumnWidth -
                    countFilteredColumnWidth - 5 /*- decorationWidth*/;
    setColumnWidth(3, countColumnWidth);
    setColumnWidth(2, countFilteredColumnWidth);
    setColumnWidth(0, col0Width);
}

void Filters::resizeEvent(QResizeEvent *event)
{
    if (G::isLogger) G::log("Filters::resizeEvent");
    resizeColumns();
    QTreeWidget::resizeEvent(event);
}

void Filters::paintEvent(QPaintEvent *event)
{
    // if (G::isLogger) G::log("Filters::paintEvent");
    QTreeWidget::paintEvent(event);
}

void Filters::mousePressEvent(QMouseEvent *event)
{
/*
    Single mouse click on item triggers expand/collapse.  The decoraton
    (arrow head) is shown but its behavior is disabled as it does not
    support solo mode.
*/
    if (G::isLogger) G::log("Filters::mousePressEvent");
    if (G::mode == "Compare") {
        G::popup->showPopup("Filters are unavailable while in Compare Mode", 3000);
    }
    if (buildingFilters) return;
    QPoint p = event->pos();
    QModelIndex idx = indexAt(p);
    // ignore if click below filter items
    if (!idx.isValid()) {
        QTreeWidget::mousePressEvent(event);
        return;
    }

    // ignore mouse clicks on decoration to the header
    if (p.x() < indentation) p.setX(indentation);
    QTreeWidgetItem *item = itemFromIndex(idx);
    bool isLeftBtn = event->button() == Qt::LeftButton;
    bool isHdr = idx.parent() == QModelIndex();
    bool isEmptyHdr = isHdr && item->childCount() == 0;
    if (isEmptyHdr) return;
    bool isValid = idx.isValid();
    /*
    qDebug() << "Filters::mousePressEvent" << p
             << "isLeftBtn =" << isLeftBtn
             << "isHdr =" << isHdr
             << "notIndentation =" << notIndentation
             << "isValid =" << isValid
                ; //*/
    bool isCtrlModifier = event->modifiers() & Qt::ControlModifier;

    /* Opt+click EXCLUDES, and a plain click on something already excluded clears it.
       Both are handled here, BEFORE the base class runs, because the base class would
       toggle the checkbox as well and we would be fighting it for the same state.
       Opt is the modifier the mask combine tools already use for "subtract", so the
       gesture is one the user has met. */
    if (isLeftBtn && !isHdr && isValid && isFilterableItem(item)
        && (categoriesFrom == FromCatalog || (G::allMetadataAttempted && !buildingFilters))) {
        const bool isAltModifier = event->modifiers() & Qt::AltModifier;
        const bool isShiftModifier = event->modifiers() & Qt::ShiftModifier;
        const Qt::CheckState now = item->checkState(0);
        /* Handled here, before the base class, for the same reason as Opt+click: the base
           class would toggle the clicked checkbox as well and we would be fighting it for
           the state we just set. */
        /*  SWALLOW THE RELEASE TOO on every path that has already settled the state
            here. QAbstractItemView emits itemClicked from the RELEASE, against the index
            it still has pressed, and itemClickedSignal then treats it as a click on the
            item's text and toggles the box a second time. On a Shift+range that put the
            clicked item back on after the range had just cleared it -- the reported
            "item1 remained checked" -- and on every one of these paths it ran MW::filterChange
            a SECOND time over the whole model, which is filtration the user waits for
            twice to arrive where they already were. */
        if (isShiftModifier && !isAltModifier && applyRangeCheck(item)) {
            swallowNextRelease = true;
            return;
        }
        if (isAltModifier) {
            setItemFilterState(item, now == Qt::PartiallyChecked ? Qt::Unchecked
                                                                 : Qt::PartiallyChecked);
            swallowNextRelease = true;
            return;
        }
        if (now == Qt::PartiallyChecked) {
            /* Without this, Qt's own two-state toggle turns an exclusion into an
               inclusion -- the opposite of what the user asked for -- and there would be
               no way to simply clear one. */
            setItemFilterState(item, Qt::Unchecked);
            swallowNextRelease = true;
            return;
        }
    }

    if (isLeftBtn && isHdr && isValid /*&& notIndentation*/) {
        hdrJustClicked = true;
        if (isSolo && !isCtrlModifier) {
            if (isExpanded(idx)) {
                bool otherHdrWasExpanded = otherHdrExpanded(idx);
                collapseAll();
                if (otherHdrWasExpanded) expand(idx);
            }
            else {
                collapseAll();
                expand(idx);
            }
        }
        else {
            isExpanded(idx) ? collapse(idx) : expand(idx);
        }
        // set decoration
        if (isExpanded(idx))
            item->setIcon(0, QIcon(":/images/branch-open-winnow.png"));
        else
            item->setIcon(0, QIcon(":/images/branch-closed-winnow.png"));
    }
    else {
        hdrJustClicked = false;
    }

    QTreeWidget::mousePressEvent(event);
}

void Filters::mouseReleaseEvent(QMouseEvent * event)
{
/*
    Ignore header clicks if solo mode, which can change the row under
    the cursor point between press and release.
*/
    if (G::isLogger) G::log("Filters::mouseReleaseEvent");
    if (swallowNextRelease) {
        swallowNextRelease = false;
        return;
    }
    if (!hdrJustClicked) QTreeWidget::mouseReleaseEvent(event);
}

void Filters::howThisWorks()
{
/*
    ONE HELP WINDOW FOR THE WHOLE PANEL. The query grammar lives here too -- brackets,
    precedence and two spellings of NOT are more than the Search row's tooltip holds, and
    a second ? beside the row would ask the user to guess which one answers their
    question. The ? in the Filters title bar is where someone looking at the panel looks.
*/
    if (G::isLogger) G::log("Filters::howThisWorks");
    QRect r = QRect(mapToGlobal(QPoint(0, 0)), size());
    new HtmlWindow("Winnow - How filters work",
                   ":/Docs/filtershelp.html",
                   QSize(720, 640), r, window());
}

void Filters::editSearchText()
{
/*
    Open the editor on the Search row -- what F2 lands on, in either scope.

    The category is expanded and scrolled to first: the row is an ordinary tree item, so
    an editor opened on a collapsed or scrolled-away category is an editor the user
    cannot see, which is exactly what the shortcut felt like while the category was
    hidden behind the panel's own search box.
*/
    if (G::isLogger) G::log("Filters::editSearchText");
    setRowHidden(indexOfTopLevelItem(search), QModelIndex(), false);
    expandItem(search);
    scrollToItem(search);
    editItem(searchTrue, 0);
}

QString Filters::diagnostics()
{
    if (G::isLogger) G::log("Filters::diagnostics");

    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', "Filters Diagnostics");

    rpt << "\n\n";
    rpt << "Filters:\n";
    int catWidth = 30;
    int itemWidth = 40;
    int chkWidth = 9;
    int countWidth = 17;
    // column headers
    rpt.setFieldAlignment(QTextStream::AlignLeft);
    rpt.setFieldWidth(catWidth);
    rpt << "Category";
    rpt.setFieldWidth(itemWidth);
    rpt << "Item";
    rpt.setFieldWidth(chkWidth);
    rpt << "Checked";
    rpt.setFieldWidth(countWidth);
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rpt << "Filtered Count" << "Unfiltered Count";
    rpt << "\n";
    rpt.reset();
    // data
    QTreeWidgetItemIterator it(this);
    while (*it) {
        if ((*it)->parent() /*&& (*it) != searchTrue*/) {
            rpt.setFieldAlignment(QTextStream::AlignLeft);
            rpt.setFieldWidth(catWidth);
            rpt << (*it)->parent()->text(0);
            rpt.setFieldWidth(itemWidth);
            rpt << (*it)->text(0);
            rpt.setFieldWidth(chkWidth);
            rpt << QVariant((*it)->checkState(0) == Qt::Checked).toString();
            rpt.setFieldAlignment(QTextStream::AlignRight);
            rpt.setFieldWidth(countWidth);
            rpt << (*it)->data(2, Qt::EditRole).toString();
            rpt << (*it)->data(3, Qt::EditRole).toString();
            rpt << "\n";
        }
        ++it;
    }




    return reportString;
}
