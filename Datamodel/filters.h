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
    QTreeWidgetItem *ratings;
    QTreeWidgetItem *labels;
    QTreeWidgetItem *types;
    QTreeWidgetItem *models;
    QTreeWidgetItem *titles;
    QTreeWidgetItem *lenses;
    QTreeWidgetItem *keywords;
    QTreeWidgetItem *creators;
    QTreeWidgetItem *focalLengths;
    QTreeWidgetItem *years;
    QTreeWidgetItem *days;

    QTreeWidgetItem *cjf;   // category just filtered

    QString catSearch = "Search";
    QString catRefine = "Refine";
    QString catPick = "Picks";
    QString catRating = "Ratings";
    QString catLabel = "Color classes";
    QString catType = "File types";
    QString catYear = "Years";
    QString catDay = "Days";
    QString catModel = "Camera models";
    QString catLens = "Lenses";
    QString catFocalLength = "Focal lengths";
    QString catTitle = "Titles";
    QString catKeyword = "Keywords";
    QString catCreator = "Creators";

    QMap<QString,int> filterCategoryToDmColumn;
//    QHash<QString,QString>categories;
    QLabel *filterLabel;
    QProgressBar *bfProgressBar;
    QFrame *msgFrame;

    void createFilter(QTreeWidgetItem *cat, QString name);
    void createPredefinedFilters();
    void createDynamicFilters();
    void removeChildrenDynamicFilters();
    void addCategoryFromData(QStringList itemList, QTreeWidgetItem *category);
//    void addCategoryFromData(QMap<QString, QString> itemMap, QTreeWidgetItem *category);
    void setCategoryBackground(const int &a, const int &b);
    void setCategoryBackground(QTreeWidgetItem *cat);
    void setSearchNewFolder();
    void disableColorZeroCountItems();
    void disableAllItems(bool disable);
    void setProgressBarStyle();
    bool isOnlyMostRecentDayChecked();
    void totalColumnToUse(bool combineRawJpg);

    bool combineRawJpg;
    bool buildingFilters = false;
    bool filtersBuilt = false;
    bool isSolo = true;
    QString buildingFiltersMsg = "Building filters. Press \"Esc\" to stop.";

    QString searchString = "";
    QStringList ignoreSearchStrings;
    QString enterSearchString;
    bool itemCheckStateHasChanged = false;

signals:
    void filterChange(QString source);
    void searchStringChange(QString searchString);

public slots:

    bool isAnyFilter();
    void setCatFiltering();
    bool isCatFiltering(QTreeWidgetItem *item);
    void disableEmptyCat();
    void invertFilters();
    void clearAll();
    void checkItem(QTreeWidgetItem *par, QString itemName, Qt::CheckState state);
    void uncheckAllFilters();
    void uncheckTypesFilters();
    void expandAllFilters();
    void collapseAllFilters();
    void toggleExpansion();
    void checkPicks();
    void updateProgress(int progress);
    void startBuildFilters(bool isReset = false);
    void finishedBuildFilters();
    void loadedDataModel(bool isLoaded);
    void setSoloMode(bool isSolo);
    bool otherHdrExpanded(QModelIndex thisIdx);

public slots:
    void dataChanged(const QModelIndex &topLeft,
                     const QModelIndex &bottomRight,
                     const QVector<int> &roles = QVector<int>()) override;
    void itemClickedSignal(QTreeWidgetItem *item, int column);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
//    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void resizeColumns();
    QLinearGradient categoryBackground;
    QFont categoryFont;
    QFont searchDefaultTextFont;
    QColor searchDefaultTextColor;
    QColor hdrIsFilteringColor;
    QColor hdrIsEmptyColor;
    int indentation;
    bool hdrJustClicked;
    QModelIndex searchTrueIdx;
};

#endif // FILTERS_H
