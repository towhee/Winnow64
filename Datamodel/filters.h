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
    QTreeWidgetItem *search;
    QTreeWidgetItem *searchText;
    QTreeWidgetItem *refine;
    QTreeWidgetItem *refineFalse;
    QTreeWidgetItem *refineTrue;
    QTreeWidgetItem *picks;
    QTreeWidgetItem *picksFalse;
    QTreeWidgetItem *picksTrue;
    QTreeWidgetItem *picksReject;
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
    void setCategoryBackground(const int &a, const int &b);
    void setChildFlags();

    QStringList ignoreSearchStrings;
    QString defaultSearchString;

signals:
    void filterChange(QString source);
    void searchStringChange(QString searchString);

public slots:

    bool isAnyFilter();
    void invertFilters();
    void clearAll();
    void uncheckAllFilters();
    void uncheckTypesFilters();
    void expandAllFilters();
    void collapseAllFilters();
    void checkPicks(bool check);
//    void resizeColumns();

protected:
    void resizeEvent(QResizeEvent *event);
    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent *event);

private:
    void resizeColumns();
    QLinearGradient categoryBackground;
    QFont categoryFont;
    int indentation;

    bool itemHasChanged;
    QString searchString = "";
    void itemChangedSignal(QTreeWidgetItem *item, int column);
    void itemClickedSignal(QTreeWidgetItem *item, int column);
};

#endif // FILTERS_H
