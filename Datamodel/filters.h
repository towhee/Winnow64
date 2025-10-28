#ifndef FILTERS_H
#define FILTERS_H

#include <QtWidgets>
#include "Utilities/utilities.h"
#include "ui_filtershelp.h"
//#include "Datamodel/datamodel.h"

class Filters : public QTreeWidget
{
    Q_OBJECT
public:
    Filters(QWidget *parent);
    QTreeWidgetItem *search;
    QTreeWidgetItem *searchTrue;
    QTreeWidgetItem *searchFalse;
    QTreeWidgetItem *picks;
    QTreeWidgetItem *ratings;
    QTreeWidgetItem *labels;
    QTreeWidgetItem *types;
    QTreeWidgetItem *folders;
    QTreeWidgetItem *models;
    QTreeWidgetItem *titles;
    QTreeWidgetItem *lenses;
    QTreeWidgetItem *keywords;
    QTreeWidgetItem *focalLengths;
    QTreeWidgetItem *years;
    QTreeWidgetItem *days;
    QTreeWidgetItem *creators;
    QTreeWidgetItem *missingThumbs;
    QTreeWidgetItem *compare;

    QTreeWidgetItem *activeCategory;   // category just filtered

    QString catSearch = "Search";
    QString catPick = "Picks";
    QString catRating = "Ratings";
    QString catLabel = "Color classes";
    QString catType = "File types";
    QString catFolder = "Folders";
    QString catYear = "Years";
    QString catDay = "Days";
    QString catModel = "Camera models";
    QString catLens = "Lenses";
    QString catFocalLength = "Focal lengths";
    QString catTitle = "Titles";
    QString catKeyword = "Keywords";
    QString catCreator = "Creators";
    QString catMissingThumbs = "Missing embedded thumbs";
    QString catCompare = "Duplicates found";

    QMap<QString,int> filterCategoryToDmColumn;
//    QHash<QString,QString>categories;
    QLabel *filterLabel;
    QProgressBar *bfProgressBar;
    QFrame *msgFrame;

    void createFilter(QTreeWidgetItem *cat, QString name);
    void createPredefinedFilters();
    void createDynamicFilters();
    void removeChildrenDynamicFilters();
//    void updateCategoryItems(QStringList itemList, QTreeWidgetItem *category);
    void updateSearchCategoryCount(QMap<QString, int> itemMap, bool isFiltered);
    void updateCategoryItems(QMap<QString, int> itemMap, QTreeWidgetItem *category);
    void addCategoryItems(QMap<QString, int> itemMap, QTreeWidgetItem *category);
    void updateFilteredCountPerItem(QMap<QString, int> itemMap, QTreeWidgetItem *category);
    void updateUnfilteredCountPerItem(QMap<QString, int> itemMap, QTreeWidgetItem *category);
    void updateZeroCountCheckedItems(QMap<QString, int> itemMap, QTreeWidgetItem *category);
    void setCategoryBackground(const int &a, const int &b);
    void setCategoryBackground(QTreeWidgetItem *cat);
    void setSearchNewFolder();
    void setCategoryFilterStatus(QTreeWidgetItem *item);
    bool isPredefinedNonZeroCount(QString itemName);
    void disableColorZeroCountItems();
    void disableAllItems(bool disable);
    void disableAllHeaders(bool disable);
    void disableColorAllHeaders(bool disable);
    void setProgressBarStyle();
    bool isOnlyMostRecentDayChecked();

    QString diagnostics();

    bool combineRawJpg;
    bool buildingFilters = false;
    bool filtersBuilt = false;
    // bool abort = false;
    bool isReset = true;
    bool isSolo = true;
    QString buildingFiltersMsg = "Building filters.";

    QString searchString = "";
    QStringList ignoreSearchStrings;
    QString enterSearchString;
    bool itemCheckStateHasChanged = false;

signals:
    void filterChange(QString source);
    void searchStringChange(QString searchString);

public slots:

    bool isAnyFilter();
    void setEachCatTextColor();
    bool isCatFiltering(QTreeWidgetItem *item);
    void reset();
    void save();
    void restore();
    void reportSaved();
    void disable();
    void enable();
    void disableEmptyCat();
    void invertFilters();
    void clearAll();
    void checkItem(QTreeWidgetItem *par, QString itemName, Qt::CheckState state);
    void uncheckAllFilters();
    void expandAllFilters();
    void collapseAllFilters();
    void collapseAllFiltersExceptSearch();
    void toggleExpansion();
    void setPicksState(bool isChecked);
    void setRatingState(QString rating, bool isChecked);
    void setLabelState(QString label, bool isChecked);  // color class red, yellow...
    bool isRatingChecked(QString rating);
    bool isLabelChecked(QString label);
    bool isTitleChecked(QString title);
    bool isCreatorChecked(QString creator);
    bool isAnyCatItemChecked(QTreeWidgetItem *category);
    void updateProgress(int progress);
    void startBuildFilters(bool isReset = false);
    void finishedBuildFilters();
    void loadingDataModel(bool isLoaded);
    void loadingDataModelFailed();
    void setSoloMode(bool isSolo);
    bool otherHdrExpanded(QModelIndex thisIdx);
    void dataChanged(const QModelIndex &topLeft,
                     const QModelIndex &bottomRight,
                     const QVector<int> &roles = QVector<int>()) override;
    void itemClickedSignal(QTreeWidgetItem *item, int column);
    void howThisWorks();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QMutex mutex;
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
    bool debugFilters = false;
    struct ItemState {
        QString parent;
        QString item;
    };
    QList<ItemState>itemStates;
    QString searchText;
};

#endif // FILTERS_H
