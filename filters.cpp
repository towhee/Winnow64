#include "filters.h"

Filters::Filters(QWidget *parent) : QTreeWidget(parent)
{
/* This tree widget is loaded in a QDockWidget. It contains top level items
(Categories ie Ratings, Color Classes, File types ...). For each top level item
the children are the filter choices to filter DataModel->Proxy (dm->sf). The
categories are divided into predefined (Picks, Ratings and Color Classes) and
dynamic categories based on existing metadata (File types, Camera Models, Focal
Lengths and Titles). The dynamic filter options are populated by DataModel when
folder data is loaded by addFiles and addMetadata. The actual filtering is
executed in SortFilter subclass of QSortFilterProxy (sf) in datamodel.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "Filters::Filters";
    #endif
    }
    setRootIsDecorated(true);
    setSelectionMode(QAbstractItemView::NoSelection);
    setColumnCount(2);
    setHeaderHidden(true);
    setColumnWidth(0, 200);
    setColumnWidth(1, 50);
    hideColumn(1);
//    setIndentation(10);

    categoryBackground.setStart(0, 0);
    categoryBackground.setFinalStop(0, 18);
    categoryBackground.setColorAt(0, QColor(88,88,88));
    categoryBackground.setColorAt(1, QColor(66,66,66));
    categoryFont = this->font();
    categoryFont.setBold(true);

    createPredefinedFilters();
    createDynamicFilters();

    setStyleSheet("QTreeView::item { height: 18px;}");
}

void Filters::createPredefinedFilters()
{
/* Predefined filters are edited by the user: Picks, Ratings and Color Class.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "Filters::createPredefinedFilters";
    #endif
    }
    picks = new QTreeWidgetItem(this);
    picks->setText(0, "Picks");
    picks->setFont(0, categoryFont);
    picks->setBackground(0, categoryBackground);
    picks->setBackground(1, categoryBackground);
    picks->setData(0, G::ColumnRole, G::PickedColumn);
    picksFalse = new QTreeWidgetItem(picks);
    picksFalse->setText(0, "");
    picksFalse->setCheckState(0, Qt::Unchecked);
    picksFalse->setData(1, Qt::EditRole, "false");
    picksTrue = new QTreeWidgetItem(picks);
    picksTrue->setText(0, "✓");
    picksTrue->setCheckState(0, Qt::Unchecked);
    picksTrue->setData(1, Qt::EditRole, "true");

    ratings = new QTreeWidgetItem(this);
    ratings->setText(0, "Ratings");
    ratings->setFont(0, categoryFont);
    ratings->setBackground(0, categoryBackground);
    ratings->setBackground(1, categoryBackground);
    ratings->setData(0, G::ColumnRole, G::RatingColumn);

    ratingsNone = new QTreeWidgetItem(ratings);
    ratingsNone->setText(0, "");
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
    labels->setBackground(1, categoryBackground);
    labels->setData(0, G::ColumnRole, G::LabelColumn);

    labelsNone = new QTreeWidgetItem(labels);
    labelsNone->setText(0, "");
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
    qDebug() << "Filters::createDynamicFilters";
    #endif
    }
    types = new QTreeWidgetItem(this);
    types->setText(0, "File type");
    types->setFont(0, categoryFont);
    types->setBackground(0, categoryBackground);
    types->setBackground(1, categoryBackground);
    types->setData(0, G::ColumnRole, G::TypeColumn);

    models = new QTreeWidgetItem(this);
    models->setText(0, "Camera model");
    models->setFont(0, categoryFont);
    models->setBackground(0, categoryBackground);
    models->setBackground(1, categoryBackground);
    models->setData(0, G::ColumnRole, G::CameraModelColumn);

    titles = new QTreeWidgetItem(this);
    titles->setText(0, "Title");
    titles->setFont(0, categoryFont);
    titles->setBackground(0, categoryBackground);
    titles->setBackground(1, categoryBackground);
    titles->setData(0, G::ColumnRole, G::TitleColumn);

    focalLengths = new QTreeWidgetItem(this);
    focalLengths->setText(0, "FocalLengths");
    focalLengths->setFont(0, categoryFont);
    focalLengths->setBackground(0, categoryBackground);
    focalLengths->setBackground(1, categoryBackground);
    focalLengths->setData(0, G::ColumnRole, G::FocalLengthColumn);
}

void Filters::removeChildrenDynamicFilters()
{
/* The dynamic filters (see createDynamicFilters) are rebuilt when a new
folder is selected.  This function removes any pre-existing children to
prevent duplication and orphans.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "Filters::removeChildrenDynamicFilters";
    #endif
    }
    types->takeChildren();
    models->takeChildren();
    titles->takeChildren();
    focalLengths->takeChildren();
}

void Filters::uncheckAllFilters()
{
/* Uncheck all the filter items
*/
    {
    #ifdef ISDEBUG
    qDebug() << "Filters::uncheckFilters";
    #endif
    }
    QTreeWidgetItemIterator it(this);
    while (*it) {
        if ((*it)->parent()) {
            (*it)->setCheckState(0, Qt::Unchecked);
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
/* All the values for a category are collected into a QMap object in thumbView
as the model data is added from the images in the folder.  The list is passed
here, where unique values are extracted and added to the category.  For example,
there could be multiple file types in the folder like JPG and NEF.  A QMap
object is used to the items can be sorted by key in the same order as the
tableView.  This function should only be used for dynamic categories (see
createDynamicFilters;
*/
    {
    #ifdef ISDEBUG
    qDebug() << "Filters::addCategoryFromData";
    #endif
    }
    static QTreeWidgetItem *item;
    QMap<QVariant, QString> uniqueItems;
    for (auto key : itemMap.keys())
    {
//      qDebug() << "itemMap" << key << "," << itemMap.value(key);
      if (!uniqueItems.contains(key)) uniqueItems[key] = itemMap.value(key);
    }
    for (auto key : uniqueItems.keys()) {
        item = new QTreeWidgetItem(category);
        item->setText(0, uniqueItems.value(key));
        item->setCheckState(0, Qt::Unchecked);
        item->setData(1, Qt::EditRole, key);
    }
}
