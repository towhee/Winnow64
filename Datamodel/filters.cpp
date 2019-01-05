#include "Datamodel/filters.h"

Filters::Filters(QWidget *parent) : QTreeWidget(parent)
{
/* Used to filter the datamodel, based on which items are checked in the tree.

It contains top level items (Categories ie Ratings, Color Classes, File types
...). For each top level item the children are the filter choices to filter
DataModel->Proxy (dm->sf). The categories are divided into predefined (Picks,
Ratings and Color Classes) and dynamic categories based on existing metadata
(File types, Camera Models, Focal Lengths and Titles). The dynamic filter
options are populated by DataModel when folder data is loaded by addFiles and
addMetadata. The actual filtering is executed in SortFilter subclass of
QSortFilterProxy (sf) in datamodel.


*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    setRootIsDecorated(true);
    setSelectionMode(QAbstractItemView::NoSelection);
    setColumnCount(3);
    setHeaderHidden(true);
    setColumnWidth(0, 250);
    setColumnWidth(1, 50);
    setColumnWidth(2, 50);
    setHeaderLabels({"Filter", "Value", "Count"});
    hideColumn(1);

    setIndentation(10);

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
    types->setData(0, G::ColumnRole, G::TypeColumn);

    years = new QTreeWidgetItem(this);
    years->setText(0, "Years");
    years->setFont(0, categoryFont);
    years->setBackground(0, categoryBackground);
    years->setBackground(2, categoryBackground);
    years->setData(0, G::ColumnRole, G::YearColumn);

    days = new QTreeWidgetItem(this);
    days->setText(0, "Days");
    days->setFont(0, categoryFont);
    days->setBackground(0, categoryBackground);
    days->setBackground(2, categoryBackground);
    days->setData(0, G::ColumnRole, G::DayColumn);

    models = new QTreeWidgetItem(this);
    models->setText(0, "Camera model");
    models->setFont(0, categoryFont);
    models->setBackground(0, categoryBackground);
    models->setBackground(2, categoryBackground);
    models->setData(0, G::ColumnRole, G::CameraModelColumn);

    lenses = new QTreeWidgetItem(this);
    lenses->setText(0, "Lenses");
    lenses->setFont(0, categoryFont);
    lenses->setBackground(0, categoryBackground);
    lenses->setBackground(2, categoryBackground);
    lenses->setData(0, G::ColumnRole, G::LensColumn);

    focalLengths = new QTreeWidgetItem(this);
    focalLengths->setText(0, "FocalLengths");
    focalLengths->setFont(0, categoryFont);
    focalLengths->setBackground(0, categoryBackground);
    focalLengths->setBackground(2, categoryBackground);
    focalLengths->setData(0, G::ColumnRole, G::FocalLengthColumn);

    titles = new QTreeWidgetItem(this);
    titles->setText(0, "Title");
    titles->setFont(0, categoryFont);
    titles->setBackground(0, categoryBackground);
    titles->setBackground(2, categoryBackground);
    titles->setData(0, G::ColumnRole, G::TitleColumn);

    creators = new QTreeWidgetItem(this);
    creators->setText(0, "Creators");
    creators->setFont(0, categoryFont);
    creators->setBackground(0, categoryBackground);
    creators->setBackground(2, categoryBackground);
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
    G::track(__FUNCTION__);
    if (check) {
        picksFalse->setCheckState(0, Qt::Unchecked);
        picksTrue->setCheckState(0, Qt::Checked);
    }
    else {
        picksFalse->setCheckState(0, Qt::Unchecked);
        picksTrue->setCheckState(0, Qt::Unchecked);
    }
    emit filterChange(true);
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
            (*it)->setData(2, Qt::EditRole, "test");
            (*it)->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        }
        ++it;
    }
    emit filterChange(false);
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

void Filters::resizeEvent(QResizeEvent *event)
{
    setColumnWidth(2, 45);
    setColumnWidth(0, width() - G::scrollBarThickness - 45);
    QTreeWidget::resizeEvent(event);
}
