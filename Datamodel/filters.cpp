#include "Datamodel/filters.h"

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
    setHeaderLabels({"", "Value", "Filter", "All", "Raw+Jpg"});
    header()->setDefaultAlignment(Qt::AlignCenter);
    hideColumn(1);

    indentation = 10;
    setIndentation(indentation);

    categoryBackground.setStart(0, 0);
    categoryBackground.setFinalStop(0, 18);
    categoryBackground.setColorAt(0, QColor(88,88,88));
    categoryBackground.setColorAt(1, QColor(66,66,66));
    categoryFont = this->font();
//    categoryFont.setBold(true);

    createPredefinedFilters();
    createDynamicFilters();

    setStyleSheet("QTreeView::item { height: 20px;}");

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
/* Predefined filters are edited by the user: Picks, Ratings and Color Class.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    refine = new QTreeWidgetItem(this);
    refine->setText(0, "Refine");
    refine->setFont(0, categoryFont);
    refine->setBackground(0, categoryBackground);
    refine->setBackground(2, categoryBackground);
    refine->setBackground(3, categoryBackground);
    refine->setBackground(4, categoryBackground);
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
    picks->setFont(0, categoryFont);
    picks->setBackground(0, categoryBackground);
    picks->setBackground(2, categoryBackground);
    picks->setBackground(3, categoryBackground);
    picks->setBackground(4, categoryBackground);
    picks->setData(0, G::ColumnRole, G::PickColumn);
    picksFalse = new QTreeWidgetItem(picks);
    picksFalse->setText(0, "Not Picked");
    picksFalse->setCheckState(0, Qt::Unchecked);
    picksFalse->setData(1, Qt::EditRole, "false");
    picksTrue = new QTreeWidgetItem(picks);
    picksTrue->setText(0, "Picked");
//    picksTrue->setText(0, "✓");
    picksTrue->setCheckState(0, Qt::Unchecked);
    picksTrue->setData(1, Qt::EditRole, "true");

    ratings = new QTreeWidgetItem(this);
    ratings->setText(0, "Ratings");
    ratings->setFont(0, categoryFont);
    ratings->setBackground(0, categoryBackground);
    ratings->setBackground(2, categoryBackground);
    ratings->setBackground(3, categoryBackground);
    ratings->setBackground(4, categoryBackground);
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
    labels->setFont(0, categoryFont);
    labels->setBackground(0, categoryBackground);
    labels->setBackground(2, categoryBackground);
    labels->setBackground(3, categoryBackground);
    labels->setBackground(4, categoryBackground);
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
    types->setFont(0, categoryFont);
    types->setBackground(0, categoryBackground);
    types->setBackground(2, categoryBackground);
    types->setBackground(3, categoryBackground);
    types->setBackground(4, categoryBackground);
    types->setData(0, G::ColumnRole, G::TypeColumn);

    years = new QTreeWidgetItem(this);
    years->setText(0, "Years");
    years->setFont(0, categoryFont);
    years->setBackground(0, categoryBackground);
    years->setBackground(2, categoryBackground);
    years->setBackground(3, categoryBackground);
    years->setBackground(4, categoryBackground);
    years->setData(0, G::ColumnRole, G::YearColumn);

    days = new QTreeWidgetItem(this);
    days->setText(0, "Days");
    days->setFont(0, categoryFont);
    days->setBackground(0, categoryBackground);
    days->setBackground(2, categoryBackground);
    days->setBackground(3, categoryBackground);
    days->setBackground(4, categoryBackground);
    days->setData(0, G::ColumnRole, G::DayColumn);

    models = new QTreeWidgetItem(this);
    models->setText(0, "Camera model");
    models->setFont(0, categoryFont);
    models->setBackground(0, categoryBackground);
    models->setBackground(2, categoryBackground);
    models->setBackground(3, categoryBackground);
    models->setBackground(4, categoryBackground);
    models->setData(0, G::ColumnRole, G::CameraModelColumn);

    lenses = new QTreeWidgetItem(this);
    lenses->setText(0, "Lenses");
    lenses->setFont(0, categoryFont);
    lenses->setBackground(0, categoryBackground);
    lenses->setBackground(2, categoryBackground);
    lenses->setBackground(3, categoryBackground);
    lenses->setBackground(4, categoryBackground);
    lenses->setData(0, G::ColumnRole, G::LensColumn);

    focalLengths = new QTreeWidgetItem(this);
    focalLengths->setText(0, "FocalLengths");
    focalLengths->setFont(0, categoryFont);
    focalLengths->setBackground(0, categoryBackground);
    focalLengths->setBackground(2, categoryBackground);
    focalLengths->setBackground(3, categoryBackground);
    focalLengths->setBackground(4, categoryBackground);
    focalLengths->setData(0, G::ColumnRole, G::FocalLengthColumn);

    titles = new QTreeWidgetItem(this);
    titles->setText(0, "Title");
    titles->setFont(0, categoryFont);
    titles->setBackground(0, categoryBackground);
    titles->setBackground(2, categoryBackground);
    titles->setBackground(3, categoryBackground);
    titles->setBackground(4, categoryBackground);
    titles->setData(0, G::ColumnRole, G::TitleColumn);

    creators = new QTreeWidgetItem(this);
    creators->setText(0, "Creators");
    creators->setFont(0, categoryFont);
    creators->setBackground(0, categoryBackground);
    creators->setBackground(2, categoryBackground);
    creators->setBackground(3, categoryBackground);
    creators->setBackground(4, categoryBackground);
    creators->setData(0, G::ColumnRole, G::CreatorColumn);
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
    while (*it) {
        if ((*it)->parent()) {
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
//    emit filterChange("Filters::uncheckAllFilters");
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
/* All the values for a category are collected into a QMap object in DataModel
as the model data is added from the images in the folder.  The list is passed
here, where unique values are extracted and added to the category.  For example,
there could be multiple file types in the folder like JPG and NEF.  A QMap
object is used so the items can be sorted by key in the same order as the
tableView.  This function should only be used for dynamic categories - see
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

void Filters::itemChangedSignal(QTreeWidgetItem *item, int column)
{
    bool isChild = item->parent();
    bool ok = isChild && column == 0 && G::isNewFolderLoaded && !G::buildingFilters;
    if (ok) {
        itemHasChanged = true;
        /*
        qDebug() << __FUNCTION__
                 << "G::isNewFolderLoaded" << G::isNewFolderLoaded
                 << "G::buildingFilters" << G::buildingFilters
                 << "item->text(column)" << item->text(column)
                 << "item->parent()" << item->parent()
                 << "isChild" << isChild
                 << "column" << column
                 << "itemHasChanged" << itemHasChanged
                 << "result" << result;
                 */
    }
//    emit filterChange(true);
}

void Filters::itemClickedSignal(QTreeWidgetItem *item, int column)
{
    bool isChild = item->parent();
    bool ok = isChild && column == 0
              && G::isNewFolderLoaded
              && !G::buildingFilters
              && itemHasChanged;
    if (ok) {
        /*
        qDebug() << __FUNCTION__
             << "G::isNewFolderLoaded" << G::isNewFolderLoaded
             << "G::buildingFilters" << G::buildingFilters
             << "item->text(column)" << item->text(column)
             << "item->parent()" << item->parent()
             << "isChild" << isChild
             << "column" << column
             << "itemHasChanged" << itemHasChanged
             << "result" << result;
             */
        emit filterChange("Filters::itemClickedSignal");
    }
    itemHasChanged = false;
}

void Filters::resizeEvent(QResizeEvent *event)
{
    setColumnWidth(4, 45);
    setColumnWidth(3, 45);
    setColumnWidth(2, 45);
    setColumnWidth(0, width() - G::scrollBarThickness - 90);
    QTreeWidget::resizeEvent(event);
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
