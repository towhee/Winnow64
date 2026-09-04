#ifndef FILTERS_H
#define FILTERS_H

#include <QtWidgets>
#include "Utilities/utilities.h"
#include "Cache/catalog.h"

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
    QTreeWidgetItem *isos;
    QTreeWidgetItem *years;
    QTreeWidgetItem *months;
    QTreeWidgetItem *days;
    QTreeWidgetItem *creators;
    QTreeWidgetItem *availability;
    // QTreeWidgetItem *missingThumbs;
    QTreeWidgetItem *compare;

    QTreeWidgetItem *activeCategory;   // category just filtered

    QString catSearch = "Search";
    QString catPick = "Picks";
    QString catRating = "Ratings";
    QString catLabel = "Color classes";
    QString catType = "File types";
    QString catFolder = "Folders";
    QString catYear = "Years";
    QString catMonth = "Months";
    QString catDay = "Days";
    QString catModel = "Camera models";
    QString catLens = "Lenses";
    QString catFocalLength = "Focal lengths";
    QString catIso = "ISO";
    QString catTitle = "Titles";
    QString catKeyword = "Keywords";
    QString catCreator = "Creators";
    QString catAvailability = "Availability";
    // QString catMissingThumbs = "Missing embedded thumbs";
    QString catCompare = "Duplicates found";

    QMap<QString,int> filterCategoryToDmColumn;
//    QHash<QString,QString>categories;
    QLabel *filterLabel;
    QProgressBar *bfProgressBar;
    QFrame *msgFrame;

    void createFilter(QTreeWidgetItem *cat, QString name);
    /*  THE VALUE A CHECKED ITEM IS COMPARED AGAINST, which is the item's own text for
        every category but one. SortFilter::compileFilters reads data(1, EditRole) and
        FilterPredicate compares it with what the model holds at Qt::EditRole -- so for
        Availability, whose column holds a CODE and whose item shows a WORD, the item
        must carry the code or nothing would ever match. See Catalog::availabilityCode. */
    QVariant filterValueFor(const QTreeWidgetItem *category, const QString &label) const;
    /*  Enable the Availability category only when a loaded row is something other than
        Present -- offline, missing or unreadable. It stays VISIBLE either way: hiding it
        answered a question the user never got to ask, and a category that comes and goes
        with the data is one nobody learns is there. Greyed, with its items showing, says
        "nothing here is unavailable" in the place the user is already looking. */
    void updateAvailabilityVisibility();
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

    /* Set one item to include (Qt::Checked), exclude (Qt::PartiallyChecked) or off, then
       restyle it and emit filterChange. The one way an item's filter state changes. */
    void setItemFilterState(QTreeWidgetItem *item, Qt::CheckState state);

    /* --- The Find dock's shared-category interface (G::useFindDock) -------------------
       The same tree renders both scopes: Folders from the datamodel via BuildFilters,
       and Catalog from the catalog via these. See Views/findpanel.h. */

    /* Drive the Search category from the panel's search box instead of the editable tree
       item, so one box serves both scopes. Empty text restores the placeholder, which is
       what "no search" means to the predicate. */
    void setSearchText(const QString &text);
    /* What the box should show. Named to avoid the private searchText member, which is
       the save/restore snapshot rather than the live value. */
    QString currentSearchText() const;

    /* Where the items in the tree came from. The datamodel-readiness guards
       (G::allMetadataAttempted, buildingFilters) apply only to FromDatamodel: a catalog
       query does not care whether the loaded folder has finished reading its metadata,
       and letting those guards swallow a click was what left the Find dock's Load button
       disabled after checking a category item in Catalog scope. */
    enum CategorySource { FromDatamodel, FromCatalog };
    CategorySource categorySource() const { return categoriesFrom; }

    /* Replace every dynamic category's items with what the catalog holds, and hide the
       categories the index cannot answer. Returns false when there is no catalog. */
    bool loadCatalogCategories();
    /* Undo loadCatalogCategories' hiding, so the datamodel's own categories all come back
       when the scope returns to Folders. */
    void showAllCategories();

    /* The checked/excluded items as a catalog query. The caller supplies the text; this
       fills keywords, excludeKeywords and the generic include/exclude maps. */
    void fillQuery(CatalogQuery &q) const;
    /* Whether any item is checked or excluded in a category the CATALOG can answer --
       what the Catalog footer needs to know before offering to load anything. */
    bool isAnyCatalogFilter() const;
    /* Re-read which keywords the catalog has seen under more than one parent and repaint
       the Keywords category. GUI thread only -- it queries the catalog. */
    void refreshAmbiguousKeywords();

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
    void contextMenuEvent(QContextMenuEvent *event) override;
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
    /* Paint one item for its state: excluded (struck through, red) and, in the Keywords
       category, ambiguous (amber). The two compose -- an excluded ambiguous keyword must
       still read as both. */
    void styleFilterItem(QTreeWidgetItem *item);
    /* True when this item may be included/excluded at all -- a child, enabled, and not
       one of the Search category's two fixed rows. */
    bool isFilterableItem(QTreeWidgetItem *item) const;
    /* SHIFT+CLICK RANGES, and where a range starts from. The anchor is the item last
       CHECKED, remembered as its category plus its text rather than as a pointer,
       because a category's items are destroyed and rebuilt every time the filters are
       rebuilt and a dangling pointer here would be a crash rather than a stale range. */
    QTreeWidgetItem *rangeAnchorCategory = nullptr;
    QString rangeAnchorItem;
    void noteRangeAnchor(QTreeWidgetItem *item);
    /* Include every item between the anchor and this one, or clear them all when they
       are already all included. False when there is no usable anchor in this category,
       which leaves the click to be handled as an ordinary one. */
    bool applyRangeCheck(QTreeWidgetItem *item);
    /* mousePressEvent settled the state itself, so the matching release must not reach
       the base class: it would emit itemClicked and toggle the item a second time. */
    bool swallowNextRelease = false;
    /* Keyword names the catalog has seen under more than one parent, case-folded. Empty
       also means "no catalog", in which case nothing is marked -- we do not know. */
    QSet<QString> ambiguousKeywords;
    CategorySource categoriesFrom = FromDatamodel;
    QColor itemIsExcludedColor;
    QColor itemIsAmbiguousColor;
    struct ItemState {
        QString parent;
        QString item;
        /* Qt::Checked (include) or Qt::PartiallyChecked (exclude). Unchecked items are
           not saved at all, so this is never Unchecked. */
        Qt::CheckState state = Qt::Checked;
    };
    QList<ItemState>itemStates;
    QString searchText;
};

#endif // FILTERS_H
