#include "Datamodel/filters.h"

class FiltersDelegate : public QStyledItemDelegate
{
public:
    explicit FiltersDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) { }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex  &/*index*/) const
    {
        int height = qRound(G::fontSize.toInt() * 1.7 * G::ptToPx);
        qDebug() << "FiltersDelegate  sizeHint  height =" << height
                 << "G::fontSize =" << G::fontSize
                 << "G::ptToPx =" << G::ptToPx
                 << "G::dpi =" << G::dpi
                    ;
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
    DataModel->Proxy (dm->sf). The categories are divided into predefined (Picks, Ratings
    and Color Classes) and dynamic categories based on existing metadata (File types,
    Camera Models, Focal Lengths, Titles etc).

    The tree columns are:
        0   CheckBox filter item
        1   The value to filter (hidden)
        2   The number of proxy rows containing the value
        3   The number of datamodel rows containing the value combining Raw+Jpg
        4   The number of datamodel rows containing the value

    The dynamic filter options are populated by DataModel on demand when the user filters
    or the filters dock has focus.

    The actual filtering is executed in SortFilter subclass of QSortFilterProxy (sf) in
    DataModel.

    The QTreeWidget does not support solo mode, so the decoration tree expansion is
    dissabled, and expansion/collapse is executed in the mousePressEvent.  The
    header row labels are modified with an icon to replicate the decoration arrow
    heads.

*/
    if (G::isLogger) G::log("Filters::Filters");
    viewport()->setObjectName("FiltersViewport");
    setRootIsDecorated(false);
    setSelectionMode(QAbstractItemView::NoSelection);
    setColumnCount(5);
//    setHeaderHidden(true);
    setColumnWidth(0, 250); // chkBox + description
    setColumnWidth(1, 50);  // value to filter (hidden)
    setColumnWidth(2, 50);  // number of proxy rows containing the value (filtered)
    setColumnWidth(3, 50);  // number of datamodel rows containing the value combining Raw+Jpg
    setColumnWidth(4, 50);  // number of datamodel rows containing the value

    // Headerlabel set in search header: {"", "Value", "Filter", "Raw+Jpg", "All"}
    header()->setDefaultAlignment(Qt::AlignCenter);
    QStringList hdrLabels = {"", "Value", "Filter", "All", "All"};
    this->setHeaderLabels(hdrLabels);
    /* how to add pixmap to a header
    model()->setHeaderData(0, Qt::Horizontal, QVariant::fromValue(QIcon(":/images/branch-closed-winnow.png")), Qt::DecorationRole);
    */

    // Cannot hide columns until tree fully initialized - see resizeColumns

    indentation = 14;
    setIndentation(indentation);

    hdrIsFilteringColor = QColor(Qt::red);

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

    createPredefinedFilters();
    createDynamicFilters();
    setCategoryBackground(a, b);

    setItemDelegate(new FiltersDelegate(this));
    setFocusPolicy(Qt::NoFocus);

    filterCategoryToDmColumn[" Search"] = G::SearchColumn;

    filterCategoryToDmColumn["Refine"] = G::RefineColumn;
    filterCategoryToDmColumn["Picks"] = G::PickColumn;
    filterCategoryToDmColumn["Ratings"] = G::RatingColumn;
    filterCategoryToDmColumn["Color class"] = G::LabelColumn;
    filterCategoryToDmColumn["File type"] = G::TypeColumn;
    filterCategoryToDmColumn["Years"] = G::YearColumn;
    filterCategoryToDmColumn["Days"] = G::DayColumn;
    filterCategoryToDmColumn["Camera model"] = G::CameraModelColumn;
    filterCategoryToDmColumn["Lenses"] = G::LensColumn;
    filterCategoryToDmColumn["FocalLengths"] = G::FocalLengthColumn;
    filterCategoryToDmColumn["Title"] = G::TitleColumn;
    filterCategoryToDmColumn["Keywords"] = G::KeywordsColumn;
    filterCategoryToDmColumn["Creators"] = G::CreatorColumn;

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

    connect(this, &Filters::itemClicked, this, &Filters::itemClickedSignal);
}

void Filters::createPredefinedFilters()
{
/* Predefined filters are edited by the user: Search, Picks, Ratings and Color Class.
*/
    if (G::isLogger) G::log("Filters::createPredefinedFilters");
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

    refine = new QTreeWidgetItem(this);
    refine->setText(0, "Refine");
//    refine->setFont(0, categoryFont);
    refine->setData(0, G::ColumnRole, G::RefineColumn);
    refine->setIcon(0, QIcon(":/images/branch-closed-winnow.png"));

    refineFalse = new QTreeWidgetItem(refine);
    refineFalse->setText(0, "False");
    refineFalse->setCheckState(0, Qt::Unchecked);
    refineFalse->setData(1, Qt::EditRole, false);
    refineTrue = new QTreeWidgetItem(refine);
    refineTrue->setText(0, "True");
    refineTrue->setCheckState(0, Qt::Unchecked);
    refineTrue->setData(1, Qt::EditRole, true);

    picks = new QTreeWidgetItem(this);
    picks->setText(0, "Picks");
//    picks->setFont(0, categoryFont);
    picks->setData(0, G::ColumnRole, G::PickColumn);
    picks->setIcon(0, QIcon(":/images/branch-closed-winnow.png"));

    picksFalse = new QTreeWidgetItem(picks);
    picksFalse->setText(0, "Not Picked");
    picksFalse->setCheckState(0, Qt::Unchecked);
    picksFalse->setData(1, Qt::EditRole, "false");
    picksTrue = new QTreeWidgetItem(picks);
    picksTrue->setText(0, "Picked");
    picksTrue->setCheckState(0, Qt::Unchecked);
    picksTrue->setData(1, Qt::EditRole, "true");
    picksReject = new QTreeWidgetItem(picks);
    picksReject->setText(0, "Rejected");
    picksReject->setCheckState(0, Qt::Unchecked);
    picksReject->setData(1, Qt::EditRole, "reject");

    ratings = new QTreeWidgetItem(this);
    ratings->setText(0, "Ratings");
//    ratings->setFont(0, categoryFont);
    ratings->setData(0, G::ColumnRole, G::RatingColumn);
    ratings->setIcon(0, QIcon(":/images/branch-closed-winnow.png"));

    ratingsNone = new QTreeWidgetItem(ratings);
    ratingsNone->setText(0, "No Rating");
    ratingsNone->setCheckState(0, Qt::Unchecked);
    ratingsNone->setData(1, Qt::EditRole, "");
    ratings1 = new QTreeWidgetItem(ratings);
    ratings1->setText(0, "One");
    ratings1->setCheckState(0, Qt::Unchecked);
    ratings1->setData(1, Qt::EditRole, 1);
    ratings2 = new QTreeWidgetItem(ratings);
    ratings2->setText(0, "Two");
    ratings2->setCheckState(0, Qt::Unchecked);
    ratings2->setData(1, Qt::EditRole, 2);
    ratings3 = new QTreeWidgetItem(ratings);
    ratings3->setText(0, "Three");
    ratings3->setCheckState(0, Qt::Unchecked);
    ratings3->setData(1, Qt::EditRole, 3);
    ratings4 = new QTreeWidgetItem(ratings);
    ratings4->setText(0, "Four");
    ratings4->setCheckState(0, Qt::Unchecked);
    ratings4->setData(1, Qt::EditRole, 4);
    ratings5 = new QTreeWidgetItem(ratings);
    ratings5->setText(0, "Five");
    ratings5->setCheckState(0, Qt::Unchecked);
    ratings5->setData(1, Qt::EditRole, 5);

    labels = new QTreeWidgetItem(this);
    labels->setText(0, "Color class");
//    labels->setFont(0, categoryFont);
    labels->setData(0, G::ColumnRole, G::LabelColumn);
    labels->setIcon(0, QIcon(":/images/branch-closed-winnow.png"));

    labelsNone = new QTreeWidgetItem(labels);
    labelsNone->setText(0, "No Color Class");
    labelsNone->setCheckState(0, Qt::Unchecked);
    labelsNone->setData(1, Qt::EditRole, "");
    labelsRed = new QTreeWidgetItem(labels);
    labelsRed->setText(0, "Red");
    labelsRed->setCheckState(0, Qt::Unchecked);
    labelsRed->setData(1, Qt::EditRole, "Red");
    labelsYellow = new QTreeWidgetItem(labels);
    labelsYellow->setText(0, "Yellow");
    labelsYellow->setCheckState(0, Qt::Unchecked);
    labelsYellow->setData(1, Qt::EditRole, "Yellow");
    labelsGreen = new QTreeWidgetItem(labels);
    labelsGreen->setText(0, "Green");
    labelsGreen->setCheckState(0, Qt::Unchecked);
    labelsGreen->setData(1, Qt::EditRole, "Green");
    labelsBlue = new QTreeWidgetItem(labels);
    labelsBlue->setText(0, "Blue");
    labelsBlue->setCheckState(0, Qt::Unchecked);
    labelsBlue->setData(1, Qt::EditRole, "Blue");
    labelsPurple = new QTreeWidgetItem(labels);
    labelsPurple->setText(0, "Purple");
    labelsPurple->setCheckState(0, Qt::Unchecked);
    labelsPurple->setData(1, Qt::EditRole, "Purple");
}

void Filters::createDynamicFilters()
{
/* Dynamic filters change with the model data and apply to file metadata such as
file type, camera model etc.  When a new folder is selected each dynamic filter
column is scanned for unique elements which are added to the dynamic filter
by addCategoryFromData.
*/
    if (G::isLogger) G::log("Filters::createDynamicFilters");
    types = new QTreeWidgetItem(this);
    types->setText(0, "File type");
    types->setData(0, G::ColumnRole, G::TypeColumn);
    types->setIcon(0, QIcon(":/images/branch-closed-winnow.png"));

    years = new QTreeWidgetItem(this);
    years->setText(0, "Years");
    years->setData(0, G::ColumnRole, G::YearColumn);
    years->setIcon(0, QIcon(":/images/branch-closed-winnow.png"));

    days = new QTreeWidgetItem(this);
    days->setText(0, "Days");
    days->setData(0, G::ColumnRole, G::DayColumn);
    days->setIcon(0, QIcon(":/images/branch-closed-winnow.png"));

    models = new QTreeWidgetItem(this);
    models->setText(0, "Camera model");
    models->setData(0, G::ColumnRole, G::CameraModelColumn);
    models->setIcon(0, QIcon(":/images/branch-closed-winnow.png"));

    lenses = new QTreeWidgetItem(this);
    lenses->setText(0, "Lenses");
    lenses->setData(0, G::ColumnRole, G::LensColumn);
    lenses->setIcon(0, QIcon(":/images/branch-closed-winnow.png"));

    focalLengths = new QTreeWidgetItem(this);
    focalLengths->setText(0, "FocalLengths");
    focalLengths->setData(0, G::ColumnRole, G::FocalLengthColumn);
    focalLengths->setIcon(0, QIcon(":/images/branch-closed-winnow.png"));

    titles = new QTreeWidgetItem(this);
    titles->setText(0, "Title");
    titles->setData(0, G::ColumnRole, G::TitleColumn);
    titles->setIcon(0, QIcon(":/images/branch-closed-winnow.png"));

    keywords = new QTreeWidgetItem(this);
    keywords->setText(0, "Keywords");
    keywords->setData(0, G::ColumnRole, G::KeywordsColumn);
    keywords->setIcon(0, QIcon(":/images/branch-closed-winnow.png"));

    creators = new QTreeWidgetItem(this);
    creators->setText(0, "Creators");
    creators->setData(0, G::ColumnRole, G::CreatorColumn);
    creators->setIcon(0, QIcon(":/images/branch-closed-winnow.png"));
}

void Filters::setCategoryBackground(const int &a, const int &b)
{
/*
    Sets the background gradient for the category items. This function is also called when the
    user changes the background shade in preferences.
*/
    if (G::isLogger) G::log("Filters::setCategoryBackground");
    categoryBackground.setColorAt(0, QColor(a,a,a));
    categoryBackground.setColorAt(1, QColor(b,b,b));

    search->setBackground(0, categoryBackground);
    search->setBackground(2, categoryBackground);
    search->setBackground(3, categoryBackground);
    search->setBackground(4, categoryBackground);

    refine->setBackground(0, categoryBackground);
    refine->setBackground(2, categoryBackground);
    refine->setBackground(3, categoryBackground);
    refine->setBackground(4, categoryBackground);

    picks->setBackground(0, categoryBackground);
    picks->setBackground(2, categoryBackground);
    picks->setBackground(3, categoryBackground);
    picks->setBackground(4, categoryBackground);

    ratings->setBackground(0, categoryBackground);
    ratings->setBackground(2, categoryBackground);
    ratings->setBackground(3, categoryBackground);
    ratings->setBackground(4, categoryBackground);

    labels->setBackground(0, categoryBackground);
    labels->setBackground(2, categoryBackground);
    labels->setBackground(3, categoryBackground);
    labels->setBackground(4, categoryBackground);

    types->setBackground(0, categoryBackground);
    types->setBackground(2, categoryBackground);
    types->setBackground(3, categoryBackground);
    types->setBackground(4, categoryBackground);

    years->setBackground(0, categoryBackground);
    years->setBackground(2, categoryBackground);
    years->setBackground(3, categoryBackground);
    years->setBackground(4, categoryBackground);

    days->setBackground(0, categoryBackground);
    days->setBackground(2, categoryBackground);
    days->setBackground(3, categoryBackground);
    days->setBackground(4, categoryBackground);

    models->setBackground(0, categoryBackground);
    models->setBackground(2, categoryBackground);
    models->setBackground(3, categoryBackground);
    models->setBackground(4, categoryBackground);

    lenses->setBackground(0, categoryBackground);
    lenses->setBackground(2, categoryBackground);
    lenses->setBackground(3, categoryBackground);
    lenses->setBackground(4, categoryBackground);

    focalLengths->setBackground(0, categoryBackground);
    focalLengths->setBackground(2, categoryBackground);
    focalLengths->setBackground(3, categoryBackground);
    focalLengths->setBackground(4, categoryBackground);

    titles->setBackground(0, categoryBackground);
    titles->setBackground(2, categoryBackground);
    titles->setBackground(3, categoryBackground);
    titles->setBackground(4, categoryBackground);

    keywords->setBackground(0, categoryBackground);
    keywords->setBackground(2, categoryBackground);
    keywords->setBackground(3, categoryBackground);
    keywords->setBackground(4, categoryBackground);

    creators->setBackground(0, categoryBackground);
    creators->setBackground(2, categoryBackground);
    creators->setBackground(3, categoryBackground);
    creators->setBackground(4, categoryBackground);
}

void Filters::removeChildrenDynamicFilters()
{
/*
    The dynamic filters (see createDynamicFilters) are rebuilt when a new
    folder is selected.  This function removes any pre-existing children to
    prevent duplication and orphans.
*/
    if (G::isLogger || G::isFlowLogger) G::log("Filters::removeChildrenDynamicFilters");
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

void Filters::checkPicks(bool check)
{
    if (G::isLogger) G::log("Filters::checkPicks");
    if (check) {
        picksFalse->setCheckState(0, Qt::Unchecked);
        picksTrue->setCheckState(0, Qt::Checked);
    }
    else {
        picksFalse->setCheckState(0, Qt::Unchecked);
        picksTrue->setCheckState(0, Qt::Unchecked);
    }
    emit filterChange("Filters::checkPicks");
}

void Filters::setSearchNewFolder()
{
    if (G::isLogger) G::log("Filters::setSearchNewFolder");
    searchTrue->setText(0, enterSearchString);
    searchTrue->setCheckState(0, Qt::Checked);
}

void Filters::disableColorZeroCountItems()
{
    if (G::isLogger) G::log("Filters::disableColorZeroCountItems");
//    return;
    QTreeWidgetItemIterator it(this);
    while (*it) {
        if ((*it)->parent() && (*it)->parent()->text(0) != " Search") {
           if ((*it)->text(2) == "0") (*it)->setForeground(0, QBrush(G::disabledColor));
            else (*it)->setForeground(0, QBrush(G::textColor));
        }
        ++it;
    }
}

void Filters::disableAllItems(bool disable)
{
    if (G::isLogger) G::log("Filters::disableAllItems");
    QTreeWidgetItemIterator it(this);
    while (*it) {
        if ((*it)->parent() && (*it)->parent()->text(0) != "Search") {
            (*it)->setDisabled(disable);
         }
        ++it;
    }
}

void Filters::updateProgress(int progress)
{
    if (G::isLogger) G::log("Filters::updateProgress");
    bfProgressBar->setValue(progress);
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

void Filters::setCatFiltering()
{
    /*
    Update all categories is filtering status
*/
    if (G::isLogger) G::log("Filters::setCatFiltering");
    QTreeWidgetItemIterator it(this);
    while (*it) {
        if ((*it)->parent() && (*it)->parent() != search) {
            if ((*it)->checkState(0) == Qt::Checked)
                (*it)->parent()->setForeground(0, QBrush(hdrIsFilteringColor));
        }
        else {
//            (*it)->setForeground(0, QBrush(G::textColor));
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

bool Filters::isOnlyMostRecentDayChecked()
{
/*
    This is used to sync MW filters menu chech state with Filters panel (here)
*/
    if (G::isLogger) G::log("Filters::isOnlyMostRecentDayChecked");
    int n = days->childCount();
    for (int i = 0; i < n; i++) {
        bool isChecked = days->child(i)->checkState(0);
        if ((i < n - 1) && isChecked) return false;
         if (i == n - 1) return isChecked;
    }
    return false;
}

void Filters::invertFilters()
{
    if (G::isLogger) G::log("Filters::invertFilters");
    QList<QString> catWithCheckedItems;
    QString cat = "";
    QTreeWidgetItemIterator it(this);

    // enable all items
    disableColorZeroCountItems();

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
    disableColorZeroCountItems();

    // emit filterChange();  // this is done in MW::invertFilters - which calls this function
}

void Filters::loadedDataModel(bool isLoaded)
{
    if (G::isLogger) G::log("Filters::loadedDataModel");
    if (isLoaded) {
        msgFrame->setVisible(false);
        filterLabel->setText("");
        filterLabel->setVisible(false);
        setEnabled(true);
    }
    else {
        msgFrame->setVisible(true);
        filterLabel->setText("Filters disabled while loading all metadata...");
        filterLabel->setVisible(true);
        setEnabled(false);
    }
}

void Filters::startBuildFilters()
{
    if (G::isLogger || G::isFlowLogger) G::log("Filters::startBuildFilters");
    removeChildrenDynamicFilters();
    filtersBuilt = false;
    buildingFilters = true;
    msgFrame->setVisible(true);
    filterLabel->setText(buildingFiltersMsg);
    filterLabel->setVisible(true);
    setProgressBarStyle();
    bfProgressBar->setVisible(true);
    collapseAll();
    setEnabled(false);
}

void Filters::finishedBuildFilters()
{
    if (G::isLogger) G::log("Filters::finishedBuildFilters");
    filtersBuilt = true;
    buildingFilters = false;
    filterLabel->setVisible(false);
    bfProgressBar->setValue(0);
    bfProgressBar->setVisible(false);
    msgFrame->setVisible(false);
    disableColorZeroCountItems();
    setEnabled(true);
    if (isSolo) collapseAll();
    else expandAll();
}

void Filters::clearAll()
{
/*
    Uncheck all the filter items but do not signal filter change.  This is called when a new
    folder is selected to reset the filter criteria.
*/
    if (G::isLogger || G::isFlowLogger) G::log("Filters::clearAll");
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
    if (G::isLogger || G::isFlowLogger) G::log("Filters::otherHdrExpanded");
    for (int i = 0; i < topLevelItemCount(); ++i) {
        QModelIndex idx = indexFromItem(topLevelItem(i));
        if (idx == thisIdx) continue;
        if (isExpanded(idx)) return true;
    }
    return false;
//    QTreeWidgetItemIterator it(this);
//    while (*it) {
//        if (!(*it)->parent()) {
//            (*it)->setCheckState(0, Qt::Unchecked);
//            (*it)->setData(2, Qt::EditRole, "");
//            (*it)->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
//            (*it)->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
//        }
//        ++it;
//    }
}

void Filters::uncheckAllFilters()
{
/*
    Uncheck all the filter items
*/
    if (G::isLogger) G::log("Filters::uncheckAllFilters");
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

void Filters::uncheckTypesFilters()
{
/*
    Uncheck types.  This is required when raw + jpg are either combined or not combined.
*/
    if (G::isLogger) G::log("Filters::uncheckTypesFilters");
    QTreeWidgetItemIterator it(this);
    while (*it) {
        if ((*it)->parent() == types) {
            (*it)->setCheckState(0, Qt::Unchecked);
            (*it)->setData(2, Qt::EditRole, "");
            (*it)->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
            (*it)->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
        }
        ++it;
    }
}

void Filters::expandAllFilters()
{
    if (G::isLogger) G::log("Filters::expandAllFilters");
    expandAll();
}

void Filters::collapseAllFilters()
{
    if (G::isLogger) G::log("Filters::collapseAllFilters");
    collapseAll();
}

void Filters::toggleExpansion()
{
    if (G::isLogger) G::log("Filters::toggleExpansion");
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
    qDebug() << "Filters::toggleExpansion" << "isExpanded =" << isExpanded;
}

void Filters::addCategoryFromData(QStringList itemList, QTreeWidgetItem *category)
{
/*
    All the values for a category are collected into a QMap object in DataModel as the
    model data is added from the images in the folder. The list is passed here, where
    unique values are extracted and added to the category. For example, there could be
    multiple file types in the folder like JPG and NEF. A QMap object is used so the
    items can be sorted by key in the same order as the tableView. This function should
    only be used for dynamic categories - see createDynamicFilters;
*/
    if (G::isLogger || G::isFlowLogger) G::log("Filters::addCategoryFromData", category->text(0));
    static QTreeWidgetItem *item;
//    // qt 6.2
//    QMap<QString, QString> uniqueItems;
//    for (auto key : itemMap.keys()) {
//      if (!uniqueItems.contains(key)) uniqueItems[key] = itemMap.value(key);
//    }
    itemList.sort();
    for (int i = 0; i < itemList.count(); i++) {
        QString s = itemList.at(i);
        item = new QTreeWidgetItem(category);
        item->setText(0, s);
        item->setCheckState(0, Qt::Unchecked);
        item->setData(1, Qt::EditRole, s);
    }
}

//void Filters::addCategoryFromData(QMap<QString, QString> itemMap, QTreeWidgetItem *category)
//{
//    /*
//    All the values for a category are collected into a QMap object in DataModel as the
//    model data is added from the images in the folder. The list is passed here, where
//    unique values are extracted and added to the category. For example, there could be
//    multiple file types in the folder like JPG and NEF. A QMap object is used so the
//    items can be sorted by key in the same order as the tableView. This function should
//    only be used for dynamic categories - see createDynamicFilters;
//*/
//    if (G::isLogger || G::isFlowLogger) G::log("Filters::addCategoryFromData", category->text(0));
//    static QTreeWidgetItem *item;
//    // qt 6.2
//    QMap<QString, QString> uniqueItems;
//    for (auto key : itemMap.keys()) {
//        if (!uniqueItems.contains(key)) uniqueItems[key] = itemMap.value(key);
//    }
//    for (auto key : uniqueItems.keys()) {
//        item = new QTreeWidgetItem(category);
//        item->setText(0, uniqueItems.value(key));
//        item->setCheckState(0, Qt::Unchecked);
//        item->setData(1, Qt::EditRole, key);
//    }
//}


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
//        if (G::allMetadataLoaded) {
//            qDebug() << "" << topLeft << topLeft.data();
//        }
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
    /*
    qDebug() << "\n" << "Filters::itemClickedSignal" << "Entering:   "
             << "column =" << column
             << "!item->parent() =" << !item->parent()
             << "itemCheckStateHasChanged" << itemCheckStateHasChanged             << "!G::isNewFolderLoaded =" << !G::isNewFolderLoaded
             << "G::buildingFilters =" << G::buildingFilters;
            //*/
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

    // is this category filtering after itemCheckStateHasChanged
    if (isCatFiltering(item->parent())) {
        item->parent()->setForeground(0, QBrush(hdrIsFilteringColor));
    }
    else {
        item->parent()->setForeground(0, QBrush(G::textColor));
    }

    emit filterChange("Filters::itemClickedSignal");
}

void Filters::setSoloMode(bool isSolo)
{
    if (G::isLogger) G::log("Filters::setSoloMode");
    this->isSolo = isSolo;
}

void Filters::totalColumnToUse(bool combineRawJpg)
{
    if (G::isLogger) G::log("Filters::totalColumnToUse");
    this->combineRawJpg = combineRawJpg;
}

void Filters::resizeColumns()
{
    if (G::isLogger) G::log("Filters::resizeColumns");
    hideColumn(1);
//    hideColumn(3);
    int totCol;
    if (combineRawJpg) {
        hideColumn(4);
        showColumn(3);
        totCol = 3;
    }
    else {
        hideColumn(3);
        showColumn(4);
        totCol = 4;
    }
    QFont font = this->font();
//    font.setPointSize(G::fontSize.toInt());
    QFontMetrics fm(font);
//    int decorationWidth = 25;       // the expand/collapse arrows
    int countColumnWidth = fm.boundingRect("99999").width();
    int countFilteredColumnWidth = fm.boundingRect("999999").width();
    int col0Width = viewport()->width() - countColumnWidth -
                    countFilteredColumnWidth - 5 /*- decorationWidth*/;
    setColumnWidth(totCol, countColumnWidth);
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
    // ignore mouse clicks on decoration to the header
    if (p.x() < indentation) p.setX(indentation);
    QModelIndex idx = indexAt(p);
    QTreeWidgetItem *item = itemFromIndex(idx);
    bool isLeftBtn = event->button() == Qt::LeftButton;
    bool isHdr = idx.parent() == QModelIndex();
//    bool notIndentation = p.x() >= indentation;
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
