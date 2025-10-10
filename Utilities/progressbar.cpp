#include "progressbar.h"
#include <QDebug>
#include "Main/mainwindow.h"


/* Displays a statusbar showing the metadata cache status.

The app status bar is 25 pixels high.  Create a bitmap the height of the
status bar and cacheStatusWidth wide the same color as the app status bar.
Then paint in the cache status progress in the middle of the bitmap.

(where cacheStatusWidth = 200)
  X = pnt background
  M = MetaRead Progress Strip
  P = Cache Progress Strip

0,0  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX  200,0
     XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
     XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
0,7  MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
0,9  PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
     PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
0,17 PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
     XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
     XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
0,25 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX  200,25

*/

extern MW *m1;
MW *m1;

ProgressBar::ProgressBar(QWidget *parent) : QWidget(parent)
{
/*
    There are two progress strips:
    - MetaRead progress strip
    - ImageCache progress strip
*/
    // ProgressBar is a friend of MW
    m1 = qobject_cast<MW*>(parent);
    setObjectName("Status ProgressBar");

    // coordinates
    mrHtOffset = 7;   // the offset from the top of pnt to the MetaRead progress bar
    mrHt = 2;         // the height of the MetaRead progress bar
    icHtOffset = 9;   // the offset from the top of pnt to the ImageCache progress bar
    icHt = 8;         // the height of the ImageCache progress bar
    setMinimumWidth(1);

    int b = G::backgroundShade + 20;

    // cursor color
    int cg = 120;
    int cr = cg * 0.67;
    int cb = cr;

    // metaRead color
    int mBrightness = 100;
    int mr = mBrightness * 0.87;
    int mg = mBrightness * 0.17;
    int mb = mBrightness * 0.44;

    // imageCache color
    int ig = cg * 0.8;
    int ir = ig * 0.67;
    int ib = ir;

    // target color
    int t = G::backgroundShade + 20;

    bgColor = QColor(b,b,b);                      // Lighest gray
    cursorColor = QColor(cr,cg,cb);               // Light green
    targetColor = QColor(t,t,t);                  // Lighter gray
    metaReadCacheColor = QColor(mr,mg,mb);        // Dark red
    imageCacheColor = QColor(ir,ig,ib);           // Dark green

    bgGradient = getGradient(bgColor);
    cursorGradient = getGradient(cursorColor);
    targetColorGradient = getGradient(targetColor);
    metaReadGradient = getGradient(metaReadCacheColor);
    imageCacheGradient = getGradient(imageCacheColor);
}

void ProgressBar::clearMetadataProgress(QColor bgColor)
{
    QPainter pnt(m1->progressPixmap);
    int w = m1->progressLabel->pixmap().width();
    QRect mdRect(0, mrHtOffset, w, mrHt);
    pnt.fillRect(mdRect, bgColor);
    pnt.end();
    m1->progressLabel->setPixmap(*(m1->progressPixmap));
}

void ProgressBar::clearImageCacheProgress()
{
    // paint ImageCache progress background
    QPainter pnt(m1->progressPixmap);
    QRect bgCacheRect(0, icHtOffset, m1->cacheBarProgressWidth, icHt);
    pnt.fillRect(bgCacheRect, bgGradient);
    pnt.end();
    m1->progressLabel->setPixmap(*(m1->progressPixmap));
}

void ProgressBar::setBackgroundColor(const QColor &bg)
{
/*
    The pixmap showing progress includes background with the progress bar in the middle.
    When the app background is changed this function selectively paints the
    non-progressbar part of the pixmap the new background shade.
*/
    if (G::isLogger) G::log("ProgressBar::setBackgroundColor");
    int w = m1->progressLabel->pixmap().width();     // Qt 6.2
    int h = m1->progressLabel->pixmap().height();    // Qt 6.2
    QPainter pnt(m1->progressPixmap);
    // above progress bar
    QRect bgRect(0, 0, w, icHtOffset);
    pnt.fillRect(bgRect, bg);
    // below progress bar
    QRect bgRect1(0, h - icHt, w, h);
    pnt.fillRect(bgRect1, bg);
    pnt.end();

    m1->progressLabel->setPixmap(*(m1->progressPixmap));
}

void ProgressBar::updateCursor(int item, int items)
{
    static int prevCursorPos = 0;
    int pos = prevCursorPos;

    QPainter pnt(m1->progressPixmap);
    int barWidth = m1->cacheBarProgressWidth;
    float itemWidth = static_cast<float>(barWidth) / items;
    int pxStartOld = qRound(pos * itemWidth);
    int pxWidth = static_cast<int>(itemWidth) + 1;
    if (pxWidth < minCursorWidth) pxWidth = minCursorWidth;

    // paint out the old cursor location
    QRect oldRect(pxStartOld, icHtOffset, pxWidth, icHt);
    pnt.fillRect(oldRect, imageCacheGradient);
    m1->progressLabel->setPixmap(*(m1->progressPixmap));

    pos = item;
    int pxStartNew = qRound(pos * itemWidth);

    // paint in the new cursor location
    QRect newRect(pxStartNew, icHtOffset, pxWidth, icHt);
    pnt.fillRect(newRect, cursorGradient);
    pnt.end();
    m1->progressLabel->setPixmap(*(m1->progressPixmap));

    prevCursorPos = pos;
}

void ProgressBar::updateImageCacheProgress(int fromItem,
                                           int toItem,
                                           int items,
                                           QLinearGradient gradient)
{
    QPainter pnt(m1->progressPixmap);
    int barWidth = m1->cacheBarProgressWidth;
    float itemWidth = (float)barWidth / items;
    int pxStart, pxWidth;

    // to and from can be mixed depending on direction of travel
    if (fromItem < toItem) {
        pxWidth = qRound((toItem - fromItem) * itemWidth) + itemWidth + 1;
        pxStart = qRound(fromItem * itemWidth);
        if (pxStart + pxWidth > barWidth) pxWidth = barWidth - pxStart;
    }
    else {
        pxWidth = qRound((fromItem - toItem) * itemWidth) + itemWidth + 1;
        pxStart = qRound(toItem * itemWidth);
    }
    if (pxWidth < 2) pxWidth = 2;
    if (pxStart + pxWidth > barWidth) pxWidth = barWidth - pxStart;

    /*
       qDebug()
            << "ProgressBar::updateImageCacheProgress"
            << "Total rows =" << items
            << "barWidth" << barWidth
            << "itemWidth" << itemWidth
            << "Target range from " << fromItem
            << "to" << toItem
            << "pxStart" << pxStart
            << "pxWidth" << pxWidth
            ;
            //*/

    // Done range
    QRect doneRect(pxStart, icHtOffset, pxWidth, icHt);
    pnt.fillRect(doneRect, gradient);
    pnt.end();
    m1->progressLabel->setPixmap(*(m1->progressPixmap));
}

void ProgressBar::updateMetadataCacheProgress(int item, int items)
{
    mrHt = 4;
    mrHtOffset = icHtOffset - mrHt;

    QPainter pnt(m1->progressPixmap);
    int barWidth = m1->cacheBarProgressWidth;
    float itemWidth = (float)barWidth / items;
    int pxStart, pxWidth;
    pxStart = qRound(item * itemWidth);
    pxWidth = itemWidth + 1;

    // Done range
    QRect doneRect(pxStart, mrHtOffset, pxWidth, mrHt);
    pnt.fillRect(doneRect, metaReadCacheColor);
    pnt.end();
    m1->progressLabel->setPixmap(*(m1->progressPixmap));
}

// void ProgressBar::updateDoneItem(bool isDone, int item, int items, QColor doneColor)
// {
//     QPainter pnt(m1->progressPixmap);
//     int barWidth = m1->cacheBarProgressWidth;
//     float itemWidth = (float)barWidth / items;
//     int pxStart, pxWidth;
//     pxWidth = qRound(itemWidth) + 1;
//     pxStart = qRound(item * itemWidth);
//     QLinearGradient doneGradient = getGradient(doneColor);
//     QRect doneRect(pxStart, icHtOffset, pxWidth, icHt);
//     pnt.fillRect(doneRect, doneGradient);
//     pnt.end();
//     m1->progressLabel->setPixmap(*(m1->progressPixmap));
// }

QLinearGradient ProgressBar::getGradient(QColor c1)
{
    QLinearGradient grad;
    grad.setStart(0, icHtOffset);
    grad.setFinalStop(0, icHt+icHtOffset);
    int r, g, b;
    c1.red() >= 50 ? r = c1.red() - 50 : r = 0;
    c1.green() >= 50 ? g = c1.green() - 50 : g = 0;
    c1.blue() >= 50 ? b = c1.blue() - 50 : b = 0;
    QColor c2(r, g, b);
    grad.setColorAt(0, c1);
    grad.setColorAt(1, c2);
    return grad;
}
