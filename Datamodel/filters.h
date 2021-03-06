#ifndef FILTERS_H
#define FILTERS_H

#include <QtWidgets>
#include "Main/global.h"
#include "Utilities/utilities.h"
//#include "Datamodel/datamodel.h"

class Filters : public QTreeWidget
{
    Q_OBJECT
public:
    Filters(QWidget *parent);
    QTreeWidgetItem *search;
    QTreeWidgetItem *searchTrue;
    QTreeWidgetItem *searchFalse;
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
    QLabel *filterLabel;
    QProgressBar *bfProgressBar;
    QFrame *msgFrame;

    void createPredefinedFilters();
    void createDynamicFilters();
    void removeChildrenDynamicFilters();
    void addCategoryFromData(QMap<QVariant, QString> itemMap, QTreeWidgetItem *category);
    void setCategoryBackground(const int &a, const int &b);
    void setSearchNewFolder();
    void disableZeroCountItems(bool disable);
    void disableAllItems(bool disable);
    void setProgressBarStyle();

    bool buildingFilters = false;
    bool filtersBuilt = false;
    QString buildingFiltersMsg = "Press \"Esc\" to stop.";

    QString searchString = "";
    QStringList ignoreSearchStrings;
    QString enterSearchString;
    bool itemCheckStateHasChanged = false;

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
    void toggleExpansion();
    void checkPicks(bool check);
    void updateProgress(int progress);
    void finishedBuildFilters();

private slots:
    void dataChanged(const QModelIndex &topLeft,
                     const QModelIndex &bottomRight,
                     const QVector<int> &roles = QVector<int>());
    void itemClickedSignal(QTreeWidgetItem *item, int column);

protected:
    void resizeEvent(QResizeEvent *event);
    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent *event);

private:
    void resizeColumns();
    QLinearGradient categoryBackground;
    QFont categoryFont;
    QFont searchDefaultTextFont;
    QColor searchDefaultTextColor;
    int indentation;
    QModelIndex searchTrueIdx;
};

#endif // FILTERS_H
