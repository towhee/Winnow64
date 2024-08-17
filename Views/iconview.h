#ifndef THUMBVIEW_H
#define THUMBVIEW_H

#include <QtWidgets>
#include "Datamodel/datamodel.h"
#include "iconviewdelegate.h"
#include <math.h>

class IconView : public QListView
{
    Q_OBJECT

public:
    IconView(QWidget *parent, DataModel *dm, QString objName);

    // Icon (Thumbnail) parameters
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
    double visibleCellCount;            // same as cellsPerVP

    // Viewport / page cell counts
    double cellsPerRow;                 // includes partly visible cells
    int cellsPerPageRow;                // whole cells per row
    double rowsPerVP;                   // includes partly visible cells
    int rowsPerPage;                    // whole rows only
    int cellsPerVP;                     // includes partly visible cells
    int cellsPerPage;                   // whole cells only

    bool isFirstCellPartial;
    bool isLastCellPartial;

    bool thumbSplitDrag = false;

    bool mouseOverThumbView = false;    // for zoomCursor in MW::eventFilter
    bool isMouseModifier = false;       // for zoomCursor in MW::eventFilter

    QModelIndex prevIdx;                // for zoomCursor
    double bestAspectRatio;

    enum JustifyAction {
        Shrink = 1,
        Enlarge = -1
    };

    void updateVisible(QString src);
    void zoomCursor(const QModelIndex &idx,
                    QString src,
                    bool forceUpdate = false,
                    QPoint mousePos = QPoint(-1, -1));

    IconViewDelegate *iconViewDelegate;
    QString diagnostics();

    QSize getMaxCellSize();
    QSize getCellSize();
    void setThumbParameters();

public slots:
    void updateView();
    void repaintView();
    int scrollPosition();
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
    void thumbsFitTopOrBottom(QString src = "");
    void updateThumbRectRole(const QModelIndex index, QRect iconRect);

    QModelIndex upIndex(int fromIndex);
    QModelIndex downIndex(int fromCell);
    QModelIndex pageUpIndex(int fromRow);
    QModelIndex pageDownIndex(int fromRow);

    bool isCellVisible(int row);
    void refreshThumb(QModelIndex idx, int role);
    void refreshThumbs();
    void setThumbParameters(int _thumbWidth, int _thumbHeight,
                            int _labelFontSize, bool _showThumbLabels,
                            int _badgeSize, int _iconNumberSize);

    void sortThumbs(int sortColumn, bool isReverse);

private slots:
    void wheelStopped();

protected:
    void startDrag(Qt::DropActions) override;
    void wheelEvent(QWheelEvent *event) override;
    bool event(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent*) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    bool viewportEvent(QEvent *event) override;

signals:
    void setValueSf(QModelIndex sfIdx, QVariant value, int instance, QString src,
                    int role = Qt::EditRole, int align = Qt::AlignLeft);
    void thumbClick(float xPct, float yPct);        // used in ThumbView::mousePressEvent
    void displayLoupe();
    void displayVideo();
    void updateThumbDockHeight();

private:
    DataModel *dm;

    QList<int> dragQueue;
    QPoint mousePressPos;
    int dragDistance = 0;

    Qt::KeyboardModifiers modifiers;
    float xPct;
    float yPct;

    bool isLeftMouseBtnPressed;
    // bool justMouseDoubleClicked = false;
    bool isMouseDrag;
    bool wheelSpinningOnEntry;
    QTimer wheelTimer;

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
