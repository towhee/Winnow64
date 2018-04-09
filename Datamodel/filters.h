#ifndef FILTERS_H
#define FILTERS_H

#include <QtWidgets>
#include "Main/global.h"
//#include "Datamodel/datamodel.h"

class Filters : public QTreeWidget
{
    Q_OBJECT
public:
    Filters(QWidget *parent);
    QTreeWidgetItem *refine;
    QTreeWidgetItem *refineFalse;
    QTreeWidgetItem *refineTrue;
    QTreeWidgetItem *picks;
    QTreeWidgetItem *picksFalse;
    QTreeWidgetItem *picksTrue;
    QTreeWidgetItem *ratings;
    QTreeWidgetItem *ratingsNone;
    QTreeWidgetItem *ratings1;
    QTreeWidgetItem *ratings2;
    QTreeWidgetItem *ratings3;
    QTreeWidgetItem *ratings4;
    QTreeWidgetItem *ratings5;
    QTreeWidgetItem *labels;
    QTreeWidgetItem *labelsNone;
    QTreeWidgetItem *labelsRed;
    QTreeWidgetItem *labelsYellow;
    QTreeWidgetItem *labelsGreen;
    QTreeWidgetItem *labelsBlue;
    QTreeWidgetItem *labelsPurple;
    QTreeWidgetItem *types;
    QTreeWidgetItem *models;
    QTreeWidgetItem *titles;
    QTreeWidgetItem *lenses;
    QTreeWidgetItem *creators;
    QTreeWidgetItem *focalLengths;
    QTreeWidgetItem *years;
    QTreeWidgetItem *days;

    QMap<QString,int> filterCategoryToDmColumn;

    void createPredefinedFilters();
    void createDynamicFilters();
    void removeChildrenDynamicFilters();
    void addCategoryFromData(QMap<QVariant, QString> itemMap, QTreeWidgetItem *category);

signals:
    void filterChange(bool isFilter);

public slots:

    bool isAnyFilter();
    void uncheckAllFilters();
    void expandAllFilters();
    void collapseAllFilters();
    void checkPicks(bool check);
//    void resizeColumns();

protected:
    void resizeEvent(QResizeEvent *event);

private:
    QLinearGradient categoryBackground;
    QFont categoryFont;
};

#endif // FILTERS_H
