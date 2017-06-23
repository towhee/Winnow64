#ifndef THUMBVIEW_H
#define THUMBVIEW_H

#include <QtWidgets>
#include "global.h"
#include "fstree.h"
#include "metadata.h"
#include "thumbviewdelegate.h"

class ThumbView : public QListView
{
    Q_OBJECT

public:
    enum UserRoles {
        FileNameRole = Qt::UserRole + 1,
        SortRole,
        LoadedRole,
        PickedRole,
        ThumbRectRole   // is this needed anymore ??
    };

    ThumbView(QWidget *parent, Metadata *metadata, bool iconView);

    int thumbWidth;
    int thumbHeight;
    int thumbSpacing;
    int thumbPadding;
    int labelFontSize;
    bool showThumbLabels;

    bool load(QString &dir, bool inclSubfolders);

    void selectThumb(QModelIndex idx);
    void selectThumb(int row);
    void selectThumb(QString &filePath);
    void selectNext();
    void selectPrev();
    void selectUp();
    void selectDown();
    void selectFirst();
    void selectLast();
    void selectRandom();

    int getNearestPick();
    QFileInfoList getPicks();
    bool isPick();
    QStringList getSelectedThumbsList();        //used by tags, might be useful
    QString getCurrentFilename();       //not used, but might be handy

    QStandardItemModel *thumbViewModel;
    QSortFilterProxyModel *thumbViewFilter;
    QFileInfoList thumbFileInfoList;
    QDir::SortFlags thumbsSortFlags;

    int thumbSize;
    QString filterStr;
    bool pickFilter;

protected:
    void startDrag(Qt::DropActions);
    void wheelEvent(QWheelEvent *event);
    void mousePressEvent(QMouseEvent *event);
    bool event(QEvent* event);      // key capture

private:
    void loadPrepare();
    bool initThumbs();
    void setThumbParameters();
    bool isSelectedItem();          // call before getting row or index
    void reportThumb();                             //debugging thumb roles

    int getNextRow();
    int getPrevRow();
    int getLastRow();                           //not used? Seems handy
    int getRandomRow();                         //not used? Seems handy

    QString currentViewDir;
    QDir *thumbsDir;
    QStringList *fileFilters;
    QList<QStandardItem*> *thumbList;
    QItemSelectionModel *thumbViewSelection;
    ThumbViewDelegate *thumbViewDelegate;
    QFileInfo thumbFileInfo;
    QImage emptyImg;
    Metadata *metadata;
    bool isIconView;

signals:
    void thumbClick(float xPct, float yPct);        //used in ThumbView::mousePressEvent

public slots:
    void thumbsEnlarge();
    void thumbsShrink();
    void thumbsFit();
    void invertSelection();                         //in use
    void updateThumbRectRole(const QModelIndex index, QRect iconRect);
    void refreshThumbs();
    void setThumbParameters(int thumbWidth, int thumbHeight, int thumbSpacing,
             int thumbPadding, int labelFontSize, bool showThumbLabels);

private slots:
};

#endif // THUMBVIEW_H
