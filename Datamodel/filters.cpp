#include "Datamodel/filters.h"

class FiltersDelegate : public QStyledItemDelegate
{
public:
    explicit FiltersDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) { }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex  &index) const
    {
        index.isValid();          // suppress compiler warning
        int height = qRound(G::fontSize.toInt() * 1.7 * G::ptToPx);
        return QSize(option.rect.width(), height);
    }
};

Filters::Filters(QWidget *parent) : QTreeWidget(parent)
{
/*
Used to define criteria for filtering the datamodel, based on which items are checked in
the tree.

The tree contains top level items (Categories ie Ratings, Color Classes, File types ...).
For each top level item the children are the filter choices to filter DataModel->Proxy
(dm->sf). The categories are divided into predefined (Picks, Ratings and Color Classes)
and dynamic categories based on existing metadata (File types, Camera Models, Focal
Lengths, Titles etc).

The tree columns are:
    0   CheckBox filter item
    1   The value to filter (hidden)
    2   The number of proxy rows containing the value
    3   The number of datamodel rows containing the value
    4

The dynamic filter options are populated by DataModel on demand when the user filters or
the filters dock has focus.

The actual filtering is executed in SortFilter subclass of QSortFilterProxy (sf) in
datamodel.

*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    setRootIsDecorated(true);
    setSelectionMode(QAbstractItemView::NoSelection);
    setColumnCount(5);
    setHeaderHidden(false);
    setColumnWidth(0, 250);
    setColumnWidth(1, 50);
    setColumnWidth(2, 50);
    setColumnWidth(3, 50);
    setColumnWidth(4, 50);
    setHeaderLabels({"", "Value", "Filter", "Raw+Jpg", "All"});
    header()->setDefaultAlignment(Qt::AlignCenter);
    hideColumn(1);

    indentation = 10;
    setIndentation(indentation);

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

    filterCategoryToDmColumn["Search"] = G::SearchColumn;

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
    filterCategoryToDmColumn["Creators"] = G::CreatorColumn;

    connect(this, &Filters::itemChanged, this, &Filters::itemChangedSignal);
    connect(this, &Filters::itemClicked, this, &Filters::itemClickedSignal);
}

void Filters::createPredefinedFilters()
{
/* Predefined filters are edited by the user: Search, Picks, Ratings and Color Class.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    search = new QTreeWidgetItem(this);
    search->setText(0, "Search");
    search->setFont(0, categoryFont);
    search->setText(2, "Filter");
    search->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
    search->setText(3, "Total");
    search->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
    search->setText(4, "Total");
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
    searchFalse->setText(0, "No match for search");
    searchFalse->setCheckState(0, Qt::Unchecked);
    searchFalse->setData(1, Qt::EditRole, false);

    refine = new QTreeWidgetItem(this);
    refine->setText(0, "Refine");
//    refine->setFont(0, categoryFont);
    refine->setData(0, G::ColumnRole, G::RefineColumn);

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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    types = new QTreeWidgetItem(this);
    types->setText(0, "File type");
//    types->setFont(0, categoryFont);
    types->setData(0, G::ColumnRole, G::TypeColumn);

    years = new QTreeWidgetItem(this);
    years->setText(0, "Years");
//    years->setFont(0, categoryFont);
    years->setData(0, G::ColumnRole, G::YearColumn);

    days = new QTreeWidgetItem(this);
    days->setText(0, "Days");
//    days->setFont(0, categoryFont);
    days->setData(0, G::ColumnRole, G::DayColumn);

    models = new QTreeWidgetItem(this);
    models->setText(0, "Camera model");
//    models->setFont(0, categoryFont);
    models->setData(0, G::ColumnRole, G::CameraModelColumn);

    lenses = new QTreeWidgetItem(this);
    lenses->setText(0, "Lenses");
//    lenses->setFont(0, categoryFont);
    lenses->setData(0, G::ColumnRole, G::LensColumn);

    focalLengths = new QTreeWidgetItem(this);
    focalLengths->setText(0, "FocalLengths");
//    focalLengths->setFont(0, categoryFont);
    focalLengths->setData(0, G::ColumnRole, G::FocalLengthColumn);

    titles = new QTreeWidgetItem(this);
    titles->setText(0, "Title");
//    titles->setFont(0, categoryFont);
    titles->setData(0, G::ColumnRole, G::TitleColumn);

    creators = new QTreeWidgetItem(this);
    creators->setText(0, "Creators");
//    creators->setFont(0, categoryFont);
    creators->setData(0, G::ColumnRole, G::CreatorColumn);
}

void Filters::setCategoryBackground(const int &a, const int &b)
{
/*
Sets the background gradient for the category items.  This function is also called when the
user changes the background shade in preferences.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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

    creators->setBackground(0, categoryBackground);
    creators->setBackground(2, categoryBackground);
    creators->setBackground(3, categoryBackground);
    creators->setBackground(4, categoryBackground);
}

void Filters::removeChildrenDynamicFilters()
{
/* The dynamic filters (see createDynamicFilters) are rebuilt when a new
folder is selected.  This function removes any pre-existing children to
prevent duplication and orphans.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    types->takeChildren();
    years->takeChildren();
    days->takeChildren();
    models->takeChildren();
    lenses->takeChildren();
    focalLengths->takeChildren();
    titles->takeChildren();
    creators->takeChildren();
}

void Filters::checkPicks(bool check)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    searchTrue->setText(0, enterSearchString);
    searchTrue->setCheckState(0, Qt::Checked);
}

void Filters::setDisabled(bool disable)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QTreeWidgetItemIterator it(this);
    while (*it) {
        if ((*it)->parent() && (*it)->parent()->text(0) != "Search") {
            if (disable) {
                if ((*it)->text(2) == "0") (*it)->setDisabled(true);
                else (*it)->setDisabled(false);
            }
            else (*it)->setDisabled(false);
        }
        ++it;
    }
}

void Filters::setSearchTextColor()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    if (searchTrue->text(0) == defaultSearchString) {
//        searchTrue->setFont(0, searchDefaultTextFont);
//        searchTrue->setForeground(0, searchDefaultTextColor);
//    }
//    else {
//        searchTrue->setFont(0, font());
//        searchTrue->setForeground(0, QColor(G::textShade, G::textShade, G::textShade));
//    }
}

bool Filters::isAnyFilter()
{
/*
This is used to determine the filter status in MW::updateFilterStatus
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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

void Filters::invertFilters()
{
    QList<QString> catWithCheckedItems;
    QString cat = "";
    QTreeWidgetItemIterator it(this);

    // enable all items
    setDisabled(false);

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
    setDisabled(true);

    // emit filterChange();  // this is done im MW::invertFilters - which calls this function
}

void Filters::clearAll()
{
/*
Uncheck all the filter items but do not signal filter change.  This is called when a new
folder is selected to reset the filter criteria.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
        ++it;
    }
    setSearchNewFolder();
}

void Filters::uncheckAllFilters()
{
    /* Uncheck all the filter items
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QTreeWidgetItemIterator it(this);
    while (*it) {
        if ((*it)->parent()) {
            (*it)->setCheckState(0, Qt::Unchecked);
            (*it)->setData(2, Qt::EditRole, "");
            (*it)->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
            (*it)->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    expandAll();
}

void Filters::collapseAllFilters()
{
    collapseAll();
}

void Filters::addCategoryFromData(QMap<QVariant, QString> itemMap, QTreeWidgetItem *category)
{
/* All the values for a category are collected into a QMap object in DataModel as the model
data is added from the images in the folder. The list is passed here, where unique values are
extracted and added to the category. For example, there could be multiple file types in the
folder like JPG and NEF. A QMap object is used so the items can be sorted by key in the same
order as the tableView. This function should only be used for dynamic categories - see
createDynamicFilters;
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    static QTreeWidgetItem *item;
    QMap<QVariant, QString> uniqueItems;
    for (auto key : itemMap.keys()) {
      if (!uniqueItems.contains(key)) uniqueItems[key] = itemMap.value(key);
    }
    for (auto key : uniqueItems.keys()) {
        item = new QTreeWidgetItem(category);
        item->setText(0, uniqueItems.value(key));
        item->setCheckState(0, Qt::Unchecked);
        item->setData(1, Qt::EditRole, key);
    }
}

//void Filters::dataChanged(const QModelIndex &topLeft,
//                          const QModelIndex &bottomRight,
//                          const QVector<int> &roles)
//{
//    if (topLeft == searchTrueIdx)
//        qDebug() << __FUNCTION__ << topLeft << searchTrueIdx << topLeft.data().toString();
//    else
//        QAbstractItemView::dataChanged(topLeft, bottomRight, roles);

//}

void Filters::itemChangedSignal(QTreeWidgetItem *item, int column)
{
/*
If the user clicks on the checkbox indicator of any child item then the checkbox state toggles
and itemChangedSignal is fired. The itemChangedSignal function sets the itemHasChanged flag to
true. Next the itemClickedSignal is fired. Since the itemHasChanged flag is true the function
itemClickedSignal only emits a filterChange.

If the user clicks on the text portion of the checkbox (ie "Purple" in the color class
filters) then the checkbox is not toggled and the itemChangedSignal is not fired. The
itemClickedSignal is fired and since the itemHasChanged flag is false the checkbox checkstate
is manually toggled and a filterChange is emitted.

If the user clicks on the text portion of the search checkbox then the itemClickedSignal is
fired and the itemClickedSignal function detects that the item is searchText and
itemHasChanged is false and sets the searchText cell to edit mode. The user makes an edit.
This fires the itemChangedSignal. The itemChangedSignal function knows the sender is the item
searchText but does not know if the itemchange was a change to the text or the checkbox state.
It compares the current search string to the previous value, and if they are different, then
the change was the search string.  If the search string is legal then searchStringChange is
emitted.  DataModel::searchStringChange receives the signal and updates the datamodel
searchColumn match to true or false for each row.  The filteredItemCount is updated.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // Only interested in child checkbox items
    if (column > 0 || !item->parent() || !G::isNewFolderLoaded || G::buildingFilters) return;
    /*
        qDebug() << __FUNCTION__
                 << item->parent()->text(0)
                 << item->text(0)
                 << "column =" << column
                 << "searchString =" << searchString
                 << "prevSearchString =" << prevSearchString
                 << "enquoteItem =" << enquoteItem
                    ;
//                 */

    /* Enquote the search if:
         - the search string has been edited
         - it is not the defaultSearchText or ""
         - it has not been enquoted                 */
    bool okToEnquote = item == searchTrue &&
                       enquoteItem &&
                       column == 0 &&
                       searchTrue->text(0).toLower() != prevSearchString &&
                       searchTrue->text(0) != enterSearchString &&
                       searchTrue->text(0) != "";
    if (okToEnquote) {
        enquoteItem = false;
        searchString = searchTrue->text(0).toLower();
        QString displaySearchString = "For: ";
        displaySearchString += Utilities::enquote(searchString);
        searchTrue->setText(0, displaySearchString);
        return;
    }

    // The search string edit is blank - set to defaultSearchString and update search
    /*
    qDebug() << __FUNCTION__
             << item->parent()->text(0)
             << item->text(0)
             << "column =" << column;
//                 */
    if (item == searchTrue) {
        if (searchTrue->text(0) == "") {
            prevSearchString = searchString;
            searchString = "";
            enquoteItem = false;
            searchTrue->setText(0, enterSearchString);
            emit searchStringChange(searchString);
            if (item->checkState(0) == Qt::Unchecked) return;
            emit filterChange("Filters::itemChangedSignal search text change");
            return;
        }
        // check if the search string has changed
        if (searchString != prevSearchString) {
            /*
            qDebug() << __FUNCTION__ << "searchText has changed"
                     << s << searchString;
//                         */
            emit searchStringChange(searchString);
            if (item->checkState(0) == Qt::Unchecked) return;
            emit filterChange("Filters::itemChangedSignal search text change");
            return;
        }
    }

    // got this far - must be a checkbox value change
    itemHasChanged = true;
}

void Filters::itemClickedSignal(QTreeWidgetItem *item, int column)
{
/*
If the user clicks on the checkbox indicator of any child item then the checkbox state toggles
and itemChangedSignal is fired. The itemChangedSignal function sets the itemHasChanged flag to
true. Next the itemClickedSignal is fired. Since the itemHasChanged flag is true the function
itemClickedSignal only emits a filterChange.

If the user clicks on the text portion of the checkbox (ie "Purple" in the color class
filters) then the checkbox is not toggled and the itemChangedSignal is not fired. The
itemClickedSignal is fired and since the itemHasChanged flag is false the checkbox checkstate
is manually toggled and a filterChange is emitted.

If the user clicks on the text portion of the search checkbox then the itemClickedSignal is
fired and the itemClickedSignal function detects that the item is searchText and
itemHasChanged is false and sets the searchText cell to edit mode. The user makes an edit.
This fires the itemChangedSignal. The itemChangedSignal function knows the sender is the item
searchText but does not know if the itemchange was a change to the text or the checkbox state.
It compares the current search string to the previous value, and if they are different, then
the change was the search string.  If the search string is legal then searchStringChange is
emitted.  DataModel::searchStringChange receives the signal and updates the datamodel
searchColumn match to true or false for each row.  The filteredItemCount is updated.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // Only interested in clicks on the checkbox column
    if (item->isDisabled() ||
        column > 0 ||
        !item->parent() ||
        !G::isNewFolderLoaded ||
        G::buildingFilters) return;

    /*
    qDebug() << __FUNCTION__ << item->text(0)
             << "isChild" << isChild
             << "itemHasChanged" << itemHasChanged
             << "isSearchTrue" << isSearchTrue;
//    */

    // if clicked on the search text then edit it
    if (!itemHasChanged && item == searchTrue) {
        prevSearchString = searchString;
        enquoteItem = true;
        editItem(searchTrue, 0);
        return;
    }

    // clicked on checkbox text (not the indicator) so toggle the check state
    if (!itemHasChanged && item != searchTrue) {
        if (item->checkState(0) == Qt::Unchecked) item->setCheckState(0, Qt::Checked);
        else item->setCheckState(0, Qt::Unchecked);
    }
    emit filterChange("Filters::itemClickedSignal");
    itemHasChanged = false;
}

void Filters::resizeColumns()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QFont font = this->font();
//    font.setPointSize(G::fontSize.toInt());
    QFontMetrics fm(font);
    int countColumnWidth = fm.boundingRect("99999").width();
    int countFilteredColumnWidth = fm.boundingRect("Filter ").width() + 10;
    setColumnWidth(4, countColumnWidth);
    setColumnWidth(3, countColumnWidth);
    setColumnWidth(2, countFilteredColumnWidth);
    setColumnWidth(0, width() - G::scrollBarThickness - countColumnWidth - countFilteredColumnWidth - 10);
}

void Filters::resizeEvent(QResizeEvent *event)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    resizeColumns();
    QTreeWidget::resizeEvent(event);
}

void Filters::paintEvent(QPaintEvent *event)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    resizeColumns();
    QTreeWidget::paintEvent(event);
}

void Filters::mousePressEvent(QMouseEvent *event)
{
/*
Single mouse click on item triggers expand/collapse same as clicking on decoration.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QPoint p = event->pos();
    QModelIndex idx = indexAt(p);
    if (idx.isValid() && p.x() >= indentation) {
        isExpanded(idx) ? collapse(idx) : expand(idx);
    }
    QTreeWidget::mousePressEvent(event);
}
