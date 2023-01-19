#ifndef THUMBVIEW_H
#define THUMBVIEW_H

#include <QtWidgets>
//#include "Main/global.h"
#include "File/fstree.h"
//#include "Metadata/metadata.h"
#include "Datamodel/datamodel.h"
#include "iconviewdelegate.h"
//#include "Datamodel/filters.h"
//#include "Cache/threadsafehash.h"
#include <math.h>

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
    QSize cellSize;
    int preferredThumbDockHeight;

    int firstVisibleCell;
    int midVisibleCell;
    int lastVisibleCell;
    int visibleCells;

    bool mouseOverThumbView = false;    // for zoomCursor in MW::eventFilter
    bool isMouseModifier = false;       // for zoomCursor in MW::eventFilter

    QModelIndex mouseOverIndex;         // for toggle pick
    QModelIndex prevIdx;                // for zoomCursor

    bool calcViewportRange(int row);
    void zoomCursor(const QModelIndex &idx,
                    QString src,
                    bool forceUpdate = false,
                    QPoint mousePos = QPoint(-1, -1));

    void updateLayout();
    void updateView();
    bool okToScroll();

    IconViewDelegate *iconViewDelegate;
    QString diagnostics();

    QFileInfoList getPicks();       // not being used.
    QSize getMinCellSize();
    QSize getMaxCellSize();
    QSize getCellSize();
    int getThumbSpaceWidth(int thumbSpaceHeight);
    QStringList getSelectedThumbsList();        //used by tags, might be useful, move to selection
    QString getCurrentFilePath();       // used by MW::updateStatus to check for instance clash.  Is this needed?
    int getSelectedCount();             // used by MW::updateStatus - move to selection?

    void setThumbParameters();

    int getHorizontalScrollBarOffset(int row);  // not being used
    int getVerticalScrollBarOffset(int row);    // not being used
    int getHorizontalScrollBarMax();
    int getVerticalScrollBarMax();

     QItemSelectionModel *thumbViewSelection;

    bool scrollWhenReady;

    enum JustifyAction {
        Shrink = 1,
        Enlarge = -1
    };
    double bestAspectRatio;

public slots:
    void scrollDown(int);
    void scrollUp(int);
    void scrollPageDown(int);
    void scrollPageUp(int);
    void scrollToRow(int row, QString source);
    void scrollToCurrent();
    void thumbsEnlarge();
    void thumbsShrink();
    int justifyMargin();
    void justify(JustifyAction action);
    void rejustify();
    void bestAspect();
    void thumbsFitTopOrBottom();
    void updateThumbRectRole(const QModelIndex index, QRect iconRect);

    QModelIndex upIndex();
    QModelIndex downIndex();
    QModelIndex pageUpIndex();
    QModelIndex pageDownIndex();

    int getFirstVisible();          // not being used
    int getLastVisible();           // not being used
    void scannedViewportRange();
    bool allPageIconsLoaded();      // not being used
    bool isRowVisible(int row);
    int getThumbsPerPage();

    void refreshThumb(QModelIndex idx, int role);
    void refreshThumbs();
    void setThumbParameters(int _thumbWidth, int _thumbHeight,
             int _labelFontSize, bool _showThumbLabels, int _badgeSize);

    void sortThumbs(int sortColumn, bool isReverse);

private slots:

protected:
    void startDrag(Qt::DropActions) override;
    void wheelEvent(QWheelEvent *event) override;
    bool event(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent* event) override;
    void leaveEvent(QEvent *event) override;

private:
    void initLoad();
    bool addFolderImageDataToModel();

    DataModel *dm;
    ImageCacheData *icd;

    Qt::KeyboardModifiers modifiers;
    float xPct;
    float yPct;

    bool isLeftMouseBtnPressed;
    bool isMouseDrag;
    bool isFitTopOrBottom = false;

    QRect cursorRect;
    QRect iconRect;
    QLabel *zoomFrame;
    QPoint mousePosition;

    // used during gridView resize to keep close to beginning thumb size
    int assignedIconWidth;
    bool skipResize;

    // used when invert selection
    bool selectionInverted = false;
    QModelIndex newCurrentIndex;

signals:
    void setValueSf(QModelIndex sfIdx, QVariant value, int instance, QString src,
                    int role = Qt::EditRole, int align = Qt::AlignLeft);
    void fileSelectionChange(QModelIndex current, QModelIndex previous, bool clearSelection, QString src);
    void togglePick();
    void thumbClick(float xPct, float yPct);        // used in ThumbView::mousePressEvent
    void displayLoupe();
    void updateStatus(bool, QString, QString);
    void updateThumbDockHeight();
};


#endif // THUMBVIEW_H
