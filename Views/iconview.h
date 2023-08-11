#ifndef THUMBVIEW_H
#define THUMBVIEW_H

#include <QtWidgets>
#include "File/fstree.h"
#include "Datamodel/datamodel.h"
#include "iconviewdelegate.h"
#include <math.h>

#ifdef Q_OS_MAC
//#include "Utilities/mac.h"
#endif

class IconView : public QListView
{
    Q_OBJECT

public:
    IconView(QWidget *parent, DataModel *dm, ImageCacheData *icd, QString objName);

    int iconWidth;
    int iconHeight;
    int labelFontSize = 13;
    bool showIconLabels;
    QString labelChoice;
    bool showZoomFrame;
    int badgeSize;
    int iconNumberSize;
    QSize cellSize;
    int preferredThumbDockHeight;

    int firstVisibleCell;
    int midVisibleCell;
    int lastVisibleCell;
    double visibleCellCount;

    double cellsPerRow;
    int cellsPerPageRow;
    double rowsPerVP;
    int rowsPerPage;
    int cellsPerVP;
    int cellsPerPage;

    bool thumbSplitDrag = false;

    bool mouseOverThumbView = false;    // for zoomCursor in MW::eventFilter
    bool isMouseModifier = false;       // for zoomCursor in MW::eventFilter

    QModelIndex shiftAnchorIndex;
    QModelIndex mouseOverIndex;         // for toggle pick
    QModelIndex prevIdx;                // for zoomCursor

    void updateVisible(int sfRow = -1);
    void updateVisibleCellCount();
    void zoomCursor(const QModelIndex &idx,
                    QString src,
                    bool forceUpdate = false,
                    QPoint mousePos = QPoint(-1, -1));

    void updateLayout();
    bool readyToScroll();

    IconViewDelegate *iconViewDelegate;
    QString diagnostics();

    QFileInfoList getPicks();       // not being used.
    QSize getMinCellSize();
    QSize getMaxCellSize();
    QSize getCellSize();
    double cellsInViewport();
    int midVisible();
    int getThumbSpaceWidth(int thumbSpaceHeight);
    QStringList getSelectedThumbsList();        //used by tags, might be useful, move to selection
    QString getCurrentFilePath();       // used by MW::updateStatus to check for instance clash.  Is this needed?
    int getSelectedCount();             // used by MW::updateStatus - move to selection?

    void setThumbParameters();

//    int firstRowFromScrollBars();
//    int midRowFromScrollBars();

    int getHorizontalScrollBarOffset(int row);  // not being used
    int getVerticalScrollBarOffset(int row);    // not being used
    int getHorizontalScrollBarMax();
    int getVerticalScrollBarMax();

    void mousePress(QMouseEvent *event);
    void mouseRelease(QMouseEvent *event);

    QItemSelectionModel *thumbViewSelection;

    bool scrollWhenReady;

    enum JustifyAction {
        Shrink = 1,
        Enlarge = -1
    };
    double bestAspectRatio;

public slots:
    void updateView();
    void repaintView();
    void scrollDown(int);
    void scrollUp(int);
    void scrollPageDown(int);
    void scrollPageUp(int);
    void scrollToRow(int row, QString source);
    void scrollToCurrent(QString source = "");
    void setThumbSize();
    void thumbsEnlarge();
    void thumbsShrink();
    int justifyMargin();
    void justify(JustifyAction action);
    void rejustify();
    void bestAspect();
    void thumbsFitTopOrBottom(QString src = "");
    void updateThumbRectRole(const QModelIndex index, QRect iconRect);

    QModelIndex upIndex(int fromIndex);
    QModelIndex downIndex(int fromCell);
    QModelIndex pageUpIndex(int fromRow);
    QModelIndex pageDownIndex(int fromRow);

    int getFirstVisible();          // not being used
    int getLastVisible();           // not being used
    void scannedViewportRange();
    bool allPageIconsLoaded();      // not being used
    bool isRowVisible(int row);
    int getThumbsPerPage();

//    void updateAfterThumbSplitterChange();
    void refreshThumb(QModelIndex idx, int role);
    void refreshThumbs();
    void setThumbParameters(int _thumbWidth, int _thumbHeight,
                            int _labelFontSize, bool _showThumbLabels,
                            int _badgeSize, int _iconNumberSize);

    void sortThumbs(int sortColumn, bool isReverse);

private slots:

protected:
    void startDrag(Qt::DropActions) override;
    void wheelEvent(QWheelEvent *event) override;
    bool event(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent*) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

signals:
    void setValueSf(QModelIndex sfIdx, QVariant value, int instance, QString src,
                    int role = Qt::EditRole, int align = Qt::AlignLeft);
//    void fileSelectionChange(QModelIndex current, QModelIndex previous, bool clearSelection, QString src);
    void togglePick();
    void thumbClick(float xPct, float yPct);        // used in ThumbView::mousePressEvent
    void displayLoupe();
//    void updateStatus(bool, QString, QString);
    void updateThumbDockHeight();

private:
    void initLoad();
    bool addFolderImageDataToModel();
    void waitUntilScrollReady();
    int fitBadge(int pxAvail);

    DataModel *dm;
    ImageCacheData *icd;

    QList<int> dragQueue;
    QPoint mousePressPos;
    int dragDistance = 0;

    Qt::KeyboardModifiers modifiers;
    float xPct;
    float yPct;

    bool isLeftMouseBtnPressed;
    bool isMouseDrag;

    QRect cursorRect;
    QRect iconRect;
    QLabel *zoomFrame;

    // used during gridView resize to keep close to beginning thumb size
    int assignedIconWidth;
    bool skipResize;

    // used when invert selection
    bool selectionInverted = false;
    QModelIndex newCurrentIndex;

    bool isDebug;
};


#endif // THUMBVIEW_H
