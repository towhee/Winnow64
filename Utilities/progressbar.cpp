#include "progressbar.h"
#include <QDebug>
#include "Main/mainwindow.h"


/* Displays a statusbar showing the metadata cache status.

The app status bar is 25 pixels high.  Create a bitmap the height of the
status bar and cacheStatusWidth wide the same color as the app status bar.
Then paint in the cache status progress in the middle of the bitmap.

(where cacheStatusWidth = 200, htOffset(9) + ht(8) = 17)
  X = pnt background, P = Cache Progress Area

0,0  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX  0,200
     XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
     XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
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
    // ProgressBar is a friend of MW
    m1 = qobject_cast<MW*>(parent);
    setObjectName("Status ProgressBar");

    htOffset = 9;               // the offset from the top of pnt to the progress bar
    ht = 8;                     // the height of the progress bar
    metaHtOffset = 7;
    metaHt = 2;
    setMinimumWidth(1);

    int b = G::backgroundShade + 20;

    // cursor
    int cg = 120;
    int cr = cg * 0.67;
    int cb = cr;

    // metaRead
    int mBrightness = 100;
    int mr = mBrightness * 0.87;
    int mg = mBrightness * 0.17;
    int mb = mBrightness * 0.44;

    // imageCache
    int ig = cg * 0.8;
    int ir = ig * 0.67;
    int ib = ir;

    // target
    int t = G::backgroundShade + 20;

    bgColor = QColor(b,b,b);                      // Lighest gray
    cursorColor = QColor(cr,cg,cb);               // Light green
    targetColor = QColor(t,t,t);                  // Lighter gray
    metaReadCacheColor = QColor(mr,mg,mb);           // Dark red
    // metaReadCacheColor = QColor(82, 9, 40);       // Dark red
    imageCacheColor = QColor(ir,ig,ib);           // Dark green
    // imageCacheColor = QColor(54,75,54);           // Dark green


    bgGradient = getGradient(bgColor);
    cursorGradient = getGradient(cursorColor);
    targetColorGradient = getGradient(targetColor);
    metaReadGradient = getGradient(metaReadCacheColor);
    imageCacheGradient = getGradient(imageCacheColor);
}

void ProgressBar::resetMetadataProgress(QColor bgColor)
{
    qDebug() << "ProgressBar::resetMetadataProgress";
    QPainter pnt(m1->progressPixmap);
    int w = m1->progressLabel->pixmap().width();
    QRect mdRect(0, metaHtOffset, w, metaHt);
    // QRect mdRect(0, metaHtOffset, m1->cacheBarProgressWidth, metaHt);
    pnt.fillRect(mdRect, bgColor);
    pnt.end();
    m1->progressLabel->setPixmap(*(m1->progressPixmap));
}

void ProgressBar::clearImageCacheProgress()
{
    // qDebug() << "ProgressBar::clearImageCacheProgress";
    counter = 0;

    // paint MetaRead progress background
    // resetMetadataProgress(G::backgroundColor);

    QPainter pnt(m1->progressPixmap);
    // paint ImageCache progress background
    QRect bgCacheRect(0, htOffset, m1->cacheBarProgressWidth, ht);
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
    QRect bgRect(0, 0, w, htOffset);
    pnt.fillRect(bgRect, bg);
    // below progress bar
    QRect bgRect1(0, h - ht, w, h);
    pnt.fillRect(bgRect1, bg);
    pnt.end();
//    // metadata progress line reset to zero progress
//    QRect mdRect(0, htOffset-1, w, htOffset-1);

    m1->progressLabel->setPixmap(*(m1->progressPixmap));
}

void ProgressBar::saveProgressState()
{
    int w = m1->progressLabel->pixmap().width();
    int h = m1->progressLabel->pixmap().height();
    state = m1->progressLabel->pixmap().copy(0, 0, w, h);
}

void ProgressBar::recoverProgressState()
{
    m1->progressLabel->setPixmap(state);
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
//    QLinearGradient oldGradient = getGradient(imageCacheColor);
    QRect oldRect(pxStartOld, htOffset, pxWidth, ht);
    pnt.fillRect(oldRect, imageCacheGradient);
    m1->progressLabel->setPixmap(*(m1->progressPixmap));

    pos = item;
    int pxStartNew = qRound(pos * itemWidth);

    // paint in the new cursor location
    QRect newRect(pxStartNew, htOffset, pxWidth, ht);
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
    counter++;

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
    QRect doneRect(pxStart, htOffset, pxWidth, ht);
    pnt.fillRect(doneRect, gradient);
    pnt.end();
    m1->progressLabel->setPixmap(*(m1->progressPixmap));
}

void ProgressBar::updateMetadataCacheProgress(int item, int items)
{
    metaHt = 4;
    metaHtOffset = htOffset - metaHt;

    QPainter pnt(m1->progressPixmap);
    int barWidth = m1->cacheBarProgressWidth;
    float itemWidth = (float)barWidth / items;
    int pxStart, pxWidth;
    pxStart = qRound(item * itemWidth);
    pxWidth = itemWidth + 1;

    // Done range
    QRect doneRect(pxStart, metaHtOffset, pxWidth, metaHt);
    pnt.fillRect(doneRect, metaReadCacheColor);
    pnt.end();
    m1->progressLabel->setPixmap(*(m1->progressPixmap));
}



void ProgressBar::updateDoneItem(bool isDone, int item, int items, QColor doneColor)
{
    QPainter pnt(m1->progressPixmap);
    int barWidth = m1->cacheBarProgressWidth;
    float itemWidth = (float)barWidth / items;
    int pxStart, pxWidth;
    pxWidth = qRound(itemWidth) + 1;
    pxStart = qRound(item * itemWidth);
    QLinearGradient doneGradient = getGradient(doneColor);
    QRect doneRect(pxStart, htOffset, pxWidth, ht);
    pnt.fillRect(doneRect, doneGradient);
    pnt.end();
    m1->progressLabel->setPixmap(*(m1->progressPixmap));
}

QLinearGradient ProgressBar::getGradient(QColor c1)
{
    QLinearGradient grad;
    grad.setStart(0, htOffset);
    grad.setFinalStop(0, ht+htOffset);
    int r, g, b;
    c1.red() >= 50 ? r = c1.red() - 50 : r = 0;
    c1.green() >= 50 ? g = c1.green() - 50 : g = 0;
    c1.blue() >= 50 ? b = c1.blue() - 50 : b = 0;
    QColor c2(r, g, b);
    grad.setColorAt(0, c1);
    grad.setColorAt(1, c2);
    return grad;
}
