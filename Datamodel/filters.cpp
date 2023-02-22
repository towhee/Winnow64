#include "Datamodel/filters.h"

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
    Used to define criteria for filtering the datamodel, based on which items are checked in
    the tree.

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

    int a = G::backgroundShade + 5;
    int b = G::backgroundShade - 15;

    categoryBackground.setStart(0, 0);
    categoryBackground.setFinalStop(0, 18);
    categoryBackground.setColorAt(0, QColor(a,a,a));
    categoryBackground.setColorAt(1, QColor(b,b,b));
    categoryFont = this->font();

    enterSearchString = "Enter search text...";
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
    filterCategoryToDmColumn[catYear] = G::YearColumn;
    filterCategoryToDmColumn[catDay] = G::DayColumn;
    filterCategoryToDmColumn[catModel] = G::CameraModelColumn;
    filterCategoryToDmColumn[catLens] = G::LensColumn;
    filterCategoryToDmColumn[catFocalLength] = G::FocalLengthColumn;
    filterCategoryToDmColumn[catTitle] = G::TitleColumn;
    filterCategoryToDmColumn[catKeyword] = G::KeywordsColumn;
    filterCategoryToDmColumn[catCreator] = G::CreatorColumn;

    createPredefinedFilters();
    createDynamicFilters();
    setCategoryBackground(a, b);

    setItemDelegate(new FiltersDelegate(this));
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
    searchTrue->setData(1, Qt::EditRole, "true");
    searchTrue->setFlags(searchTrue->flags() | Qt::ItemIsEditable);
    searchTrue->setFont(0, searchDefaultTextFont);
    searchTrue->setForeground(0, searchDefaultTextColor);
    searchTrue->setToolTip(0, "Search text is not case sensitive");
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
    years = new QTreeWidgetItem(this);
    days = new QTreeWidgetItem(this);
    models = new QTreeWidgetItem(this);
    lenses = new QTreeWidgetItem(this);
    focalLengths = new QTreeWidgetItem(this);
    titles = new QTreeWidgetItem(this);
    keywords = new QTreeWidgetItem(this);
    creators = new QTreeWidgetItem(this);

    createFilter(picks, catPick);
    createFilter(ratings, catRating);
    createFilter(labels, catLabel);
    createFilter(types, catType);
    createFilter(years, catYear);
    createFilter(days, catDay);
    createFilter(models, catModel);
    createFilter(lenses, catLens);
    createFilter(focalLengths, catFocalLength);
    createFilter(titles, catTitle);
    createFilter(keywords, catKeyword);
    createFilter(creators, catCreator);
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
    setCategoryBackground(years);
    setCategoryBackground(days);
    setCategoryBackground(models);
    setCategoryBackground(lenses);
    setCategoryBackground(focalLengths);
    setCategoryBackground(titles);
    setCategoryBackground(creators);
    setCategoryBackground(keywords);
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
    years->takeChildren();
    days->takeChildren();
    models->takeChildren();
    lenses->takeChildren();
    focalLengths->takeChildren();
    titles->takeChildren();
    keywords->takeChildren();
    creators->takeChildren();
}

void Filters::setPicksState()
{
/*
    Quick Menu Filter picks by setting 'Picked' true.
*/
    if (G::isLogger) G::log("Filters::checkPicks");
    if (debugFilters)
        qDebug() << "Filters::checkPicks"
                    ;
    checkItem(picks, "Picked", Qt::Checked);
    checkItem(picks, "Unpicked", Qt::Unchecked);
    checkItem(picks, "Rejected", Qt::Unchecked);
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
    // not found
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
        bool isChecked = days->child(i)->checkState(0);
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

    // is this category filtering after itemCheckStateHasChanged
    if (isCatFiltering(item->parent())) {
        item->parent()->setForeground(0, QBrush(hdrIsFilteringColor));
    }
    else {
        item->parent()->setForeground(0, QBrush(G::textColor));
    }
}

void Filters::disableColorZeroCountItems()
{
    // not being used
    if (G::isLogger) G::log("Filters::disableColorZeroCountItems");
    if (debugFilters)
        qDebug() << "Filters::disableColorZeroCountItems"
                    ;

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
        qDebug() << "Filters::disableAllItems"
                    ;
    QTreeWidgetItemIterator it(this);
    while (*it) {
        if ((*it)->parent() && (*it)->parent()->text(0) != "Search") {
            (*it)->setDisabled(disable);
         }
        ++it;
    }
}

void Filters::disableColorAllHeaders(bool disable)
{
    if (G::isLogger) G::log("Filters::disableAllItems");
    if (debugFilters)
        qDebug() << "Filters::disableColorAllHeaders"
                    ;
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
    if (G::isLogger) G::log("Filters::updateProgress");
    if (debugFilters)
        qDebug() << "Filters::updateProgress"
                    ;
    bfProgressBar->setValue(progress);
//    if (progress < 0) {
//        setHeaderLabel("");
//        return;
//    }
//    QString s = "Updating filter: " + QString::number((int)progress) + "%";
//    qDebug() << "Filters::updateProgress" << s;
//    setHeaderLabel(s);
}

void Filters::setProgressBarStyle()
{
//    bfProgressBar->setStyleSheet("QProgressBar::chunk{background-color:cadetblue;}");
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
    QTreeWidgetItemIterator it(this);
    if (searchTrue->checkState(0) == Qt::Checked && searchTrue->text(0) != enterSearchString)
        return true;
    while (*it) {
        if ((*it)->parent() && (*it) != searchTrue) {
            if ((*it)->checkState(0) == Qt::Checked) return true;
        }
        ++it;
    }
    return false;
}

void Filters::setEachCatTextColor()
{
/*
    Update all categories 'is filtering' status
*/
    if (G::isLogger) G::log("Filters::setCatFiltering");
    if (debugFilters)
        qDebug() << "Filters::setCatFiltering"
                    ;
    QTreeWidgetItemIterator it(this);
    while (*it) {
        if (!(*it)->parent() && (*it) != search) {
            if ((*it)->childCount() < 2)
                (*it)->setForeground(0, QBrush(hdrIsEmptyColor));
//            else if ((*it) == activeCategory)
//                (*it)->setForeground(0, QBrush(Qt::darkGreen));
            else {
                bool isChecked = false;
                for (int i = 0; i < (*it)->childCount(); i++) {
                    if ((*it)->child(i)->checkState(0) == Qt::Checked) {
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
            if (item->child(i)->checkState(0) == Qt::Checked) return true;
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
//            qDebug() << (*it)->text(0) << (*it)->childCount();
            if ((*it)->childCount() < 2)
                (*it)->setForeground(0, QBrush(hdrIsEmptyColor));
            else {
                (*it)->setForeground(0, G::textColor);
            }
        }
        ++it;
    }
}

void Filters::invertFilters()
{
    if (G::isLogger) G::log("Filters::invertFilters");
    if (debugFilters)
        qDebug() << "Filters::invertFilters"
                    ;
    QList<QString> catWithCheckedItems;
    QString cat = "";
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
            bool isChecked = (*it)->checkState(0);
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
                    // invert check state
                    if ((*it2)->checkState(0)) (*it2)->setCheckState(0, Qt::Unchecked);
                    else (*it2)->setCheckState(0, Qt::Checked);
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
    if (G::isLogger) G::log("Filters::loadingDataModel");
    if (debugFilters)
        qDebug() << "Filters::loadingDataModel"
                    ;
    if (isLoaded) {
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
        // Allow search to remain visible in case search selected in menu or F2 pressed
        // before or while filters are being built.
        collapseAllFiltersExceptSearch();
        setEnabled(false);
    }
}

void Filters::startBuildFilters(bool isReset)
{
    if (G::isLogger || G::isFlowLogger) G::log("Filters::startBuildFilters");
    if (debugFilters)
        qDebug() << "Filters::startBuildFilters"
                    ;
    if (isReset) removeChildrenDynamicFilters();
    filtersBuilt = false;
    buildingFilters = true;
    if (!G::allMetadataLoaded) {
        msgFrame->setVisible(true);
        filterLabel->setText(buildingFiltersMsg);
        filterLabel->setVisible(true);
//        setProgressBarStyle();
        bfProgressBar->setVisible(true);
    }
//    if (isReset) collapseAll();
    setEnabled(false);
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
//    if (isSolo) collapseAll();
//    else expandAll();
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
    QTreeWidgetItemIterator it(this);
    while (*it) {
        if ((*it)->parent()) {            
            (*it)->setCheckState(0, Qt::Unchecked);
            (*it)->setData(2, Qt::EditRole, "");
            (*it)->setData(3, Qt::EditRole, "");
            (*it)->setData(4, Qt::EditRole, "");
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

bool Filters::otherHdrExpanded(QModelIndex thisIdx)
{
/*
    Determines if there is another category expanded in the tree.  This is used to
    control solo mode in mousePressEvent.
*/
    if (G::isLogger || G::isFlowLogger) G::log("Filters::otherHdrExpanded");
    if (debugFilters)
        qDebug() << "Filters::otherHdrExpanded"
                    ;
    for (int i = 0; i < topLevelItemCount(); ++i) {
        QModelIndex idx = indexFromItem(topLevelItem(i));
        if (idx == thisIdx) continue;
        if (isExpanded(idx)) return true;
    }
    return false;
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
    QTreeWidgetItemIterator it(this);
    while (*it) {
        if ((*it)->parent()) {
            (*it)->setCheckState(0, Qt::Unchecked);
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
    collapse(indexFromItem(years));
    collapse(indexFromItem(days));
    collapse(indexFromItem(models));
    collapse(indexFromItem(lenses));
    collapse(indexFromItem(focalLengths));
    collapse(indexFromItem(titles));
    collapse(indexFromItem(keywords));
    collapse(indexFromItem(creators));
}

void Filters::toggleExpansion()
{
    if (G::isLogger) G::log("Filters::toggleExpansion");
    if (debugFilters)
        qDebug() << "Filters::toggleExpansion"
                    ;
    bool isExpanded = false;
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
    if (G::isLogger) G::log("Filters::updateCategoryItems", category->text(0));
    if (debugFilters)
        qDebug() << "Filters::updateCategoryItems"
                 << "category =" << category->text(0)
                    ;

    // if true then filtering will be cancelled
    bool oldItemChecked = false;

    // remove existing category items in filters if no longer in itemMap
    if (category->childCount()) {
        for (int i = category->childCount() - 1; i >= 0 ; i--) {
            QString s = category->child(i)->text(0);
            // count and remove from itemMap
            if (itemMap.contains(s)) {
                // update unfiltered item count
                category->child(i)->setData(3, Qt::EditRole, itemMap[s]);
                itemMap.remove(s);
            }
            // remove from filter tree unless checked item
            else {
                if (category->child(i)->checkState(0) == Qt::Checked)
                    oldItemChecked = true;
                category->removeChild(category->child(i));
//                break;
            }
        }
    }

    // add all remaining items in unique itemList to filter tree
    QTreeWidgetItem *item;
    QMapIterator<QString, int> i(itemMap);
    while (i.hasNext()) {
        i.next();
        item = new QTreeWidgetItem(category);
        item->setText(0, i.key());
        if (oldItemChecked) item->setCheckState(0, Qt::Checked);
        else item->setCheckState(0, Qt::Unchecked);
        item->setData(1, Qt::EditRole, i.key());
        item->setData(3, Qt::EditRole, i.value());
        item->setTextAlignment(2, Qt::AlignRight);
        item->setTextAlignment(3, Qt::AlignRight);
    }

    // sort the result
    category->sortChildren(0, Qt::AscendingOrder);

    // clear selection if anyRemovedItemsChecked = true
    if (oldItemChecked) {

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
    if (G::isLogger || G::isFlowLogger) G::log("Filters::addCategoryItems", category->text(0));
    if (debugFilters)
        qDebug() << "Filters::addCategoryItems"
                 << "category =" << category->text(0)
                    ;
//    qDebug() << "Filters::addCategoryItems  Category =" << category->text(0);

    // add all remaining items in unique itemList to filter tree
    QTreeWidgetItem *item;
    QMapIterator<QString, int> i(itemMap);
    while (i.hasNext()) {
        i.next();
        item = new QTreeWidgetItem(category);
        item->setText(0, i.key());
        item->setCheckState(0, Qt::Unchecked);
        item->setData(1, Qt::EditRole, i.key());
        item->setData(2, Qt::EditRole, i.value());
        item->setData(3, Qt::EditRole, i.value());
        item->setTextAlignment(2, Qt::AlignRight);
        item->setTextAlignment(3, Qt::AlignRight);
//        qDebug() << "Filters::addCategoryItems  Category =" << category->text(0) << "item =" << i.value();
    }

    // sort the result
//    category->sortChildren(0, Qt::AscendingOrder);
}

void Filters::addFilteredCountPerItem(QMap<QString, int> itemMap, QTreeWidgetItem *category)
{
/*
    All the unique values for a category are collected into a QMap object in
    BuildFilters. The list is passed here, where unique values are extracted and added to
    the category. For example, there could be multiple file types in the folder like JPG
    and NEF. A QMap object is used so the items can be sorted by key in the same order as
    the tableView. This function should only be used for dynamic categories - see
    createDynamicFilters;

    If a category item was just checked (activeCategory) then it is ignored, as the user
    may want to check another item in the same category.
*/
    if (G::isLogger || G::isFlowLogger) G::log("Filters::addFilteredCountPerItem", category->text(0));
    if (debugFilters)
        qDebug() << "Filters::addFilteredCountPerItem"
                 << "category =" << category->text(0)
                    ;
    // qDebug() << "Filters::addFilteredCountPerItem  Category =" << category->text(0);
//    if (activeCategory == category) {
//        // only ignore if there is at least one checked item
//        for (int i = 0; i < category->childCount(); i++) {
//            if (category->child(i)->checkState(0) == Qt::Checked) return;
//        }
//    }

    for (int i = 0; i < category->childCount(); i++) {
        category->child(i)->setData(2, Qt::EditRole, 0);
        QString key = category->child(i)->text(0);
        if (itemMap.contains(key))
            category->child(i)->setData(2, Qt::EditRole, itemMap.value(key));
    }

    // sort the result
    //    category->sortChildren(0, Qt::AscendingOrder);
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
//    if (G::isLogger) G::log("Filters::dataChanged");

    // checkstate has changed
    if (roles.contains(Qt::CheckStateRole)) {
        itemCheckStateHasChanged = true;
        QTreeWidget::dataChanged(topLeft, bottomRight, roles);
        return;
    }

    itemCheckStateHasChanged = false;

    // searchText has changed
    if (roles.contains(Qt::EditRole)) {
        if (topLeft.column() != 0) return;
        QTreeWidgetItem *item = itemFromIndex(topLeft);
        if (item == searchTrue) {
            // no search string
            if (searchTrue->text(0) == "") {
                searchString = "";
                searchTrue->setText(0, enterSearchString);
                emit searchStringChange(searchString);
                if (item->checkState(0) == Qt::Unchecked) return;
                emit filterChange("Filters::itemChangedSignal search text change");
                return;
            }
            // set searchString and filter
            searchString = searchTrue->text(0).toLower();
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
                 << "itemCheckStateHasChanged" << itemCheckStateHasChanged             << "!G::isNewFolderLoaded =" << !G::isNewFolderLoaded
                    ;
    // Only interested in clicks on column 0 (checkbox + text)
    if (item->isDisabled() ||
        column > 0 ||
        !item->parent() ||
        !G::isNewFolderLoaded ||
        buildingFilters)
    {
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
            itemCheckStateHasChanged = false;
            if (item->checkState(0) == Qt::Unchecked) item->setCheckState(0, Qt::Checked);
            else item->setCheckState(0, Qt::Unchecked);
        }
    }

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
    if (G::isLogger) G::log("Filters::paintEvent");
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
    if (!hdrJustClicked) QTreeWidget::mouseReleaseEvent(event);
}

void Filters::howThisWorks()
{
    if (G::isLogger) G::log("Filters::howThisWorks");

    QDialog *dlg = new QDialog;
    Ui::FiltersHelpDlg *ui = new Ui::FiltersHelpDlg;
    ui->setupUi(dlg);
    dlg->setWindowTitle("Filters Help");
    dlg->setStyleSheet(G::css);
    dlg->exec();
}
