#include "progressbar.h"
#include <QDebug>
#include "Main/global.h"
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

    htOffset = 9;               // the offset from the top of pnt to the progress bar
    ht = 8;                     // the height of the progress bar
    setMinimumWidth(1);

    bgGradient = getGradient(progressBgColor);
    imageCacheColorGradient = getGradient(progressImageCacheColor);
    currentColorGradient = getGradient(progressCurrentColor);
    targetColorGradient = getGradient(progressTargetColor);
}

void ProgressBar::clearProgress()
{
    counter = 0;
    QPainter pnt(m1->progressPixmap);
//    QLinearGradient bgGradient = getGradient(G::progressBgColor);
    QRect bgRect(0, htOffset, m1->progressWidth, ht);
    pnt.fillRect(bgRect, bgGradient);
    m1->progressLabel->setPixmap(*(m1->progressPixmap));
}

void ProgressBar::setBackgroundColor(const QColor &bg)
{
/*
    The pixmap showing progress includes background with the progress bar in the middle. When
    the app background is changed this function selectively paints the non-progressbar part of
    the pixmap the new background shade.
*/
    if (G::isLogger) G::log(__FUNCTION__);
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    int w = m1->progressLabel->pixmap()->width();   // deprecated
    int h = m1->progressLabel->pixmap()->height();  // deprecated
#else
    int w = m1->progressLabel->pixmap(Qt::ReturnByValueConstant()).width();     // Qt 6.2
    int h = m1->progressLabel->pixmap(Qt::ReturnByValueConstant()).height();    // Qt 6.2
#endif
    QPainter pnt(m1->progressPixmap);
    // above progress bar
    QRect bgRect(0, 0, w, htOffset);
    pnt.fillRect(bgRect, bg);
    // below progress bar
    QRect bgRect1(0, h - ht, w, h);
    pnt.fillRect(bgRect1, bg);
    m1->progressLabel->setPixmap(*(m1->progressPixmap));
}

void ProgressBar::saveProgressState()
{
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    int w = m1->progressLabel->pixmap()->width();
    int h = m1->progressLabel->pixmap()->height();
    state = m1->progressLabel->pixmap()->copy(0, 0, w, h);
#else
    int w = m1->progressLabel->pixmap(Qt::ReturnByValueConstant()).width();
    int h = m1->progressLabel->pixmap(Qt::ReturnByValueConstant()).height();
    state = m1->progressLabel->pixmap(Qt::ReturnByValueConstant()).copy(0, 0, w, h);
#endif
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
    int barWidth = m1->progressWidth;
    float itemWidth = static_cast<float>(barWidth) / items;
    int pxStartOld = qRound(pos * itemWidth);
    int pxWidth = static_cast<int>(itemWidth) + 1;
    if(pxWidth < minCursorWidth) pxWidth = minCursorWidth;


    // paint out the old cursor location
//    QLinearGradient oldGradient = getGradient(imageCacheColor);
    QRect oldRect(pxStartOld, htOffset, pxWidth, ht);
    pnt.fillRect(oldRect, imageCacheColorGradient);
//    pnt.fillRect(oldRect, oldGradient);
    m1->progressLabel->setPixmap(*(m1->progressPixmap));

    pos = item;
    int pxStartNew = qRound(pos * itemWidth);

    // paint in the new cursor location
//    QLinearGradient newGradient = getGradient(currentColor);
    QRect newRect(pxStartNew, htOffset, pxWidth, ht);
    pnt.fillRect(newRect, currentColorGradient);
//    pnt.fillRect(newRect, newGradient);
    m1->progressLabel->setPixmap(*(m1->progressPixmap));

    prevCursorPos = pos;
}

void ProgressBar::updateProgress(int fromItem,
                                 int toItem,
                                 int items,
                                 QLinearGradient gradient)
{
    counter++;

    QPainter pnt(m1->progressPixmap);
    int barWidth = m1->progressWidth;
    float itemWidth = (float)barWidth / items;
    int pxStart, pxWidth;

    // to and from can be mixed depending on direction of travel
    if(fromItem < toItem) {
        pxWidth = qRound((toItem - fromItem) * itemWidth) + 1;
        pxStart = qRound(fromItem * itemWidth);
        if(pxStart + pxWidth > barWidth) pxWidth = barWidth - pxStart;
    }
    else {
        pxWidth = qRound((fromItem - toItem) * itemWidth) + 1;
        pxStart = qRound(toItem * itemWidth);
    }
    if(pxWidth < 2) pxWidth = 2;
    if(pxStart + pxWidth > barWidth) pxWidth = barWidth - pxStart;

/*    qDebug() << "Target range from =" << fromItem
             << "to" << toItem
             << "itemWidth" << itemWidth
             << "pxStart" << pxStart
             << "pxWidth" << pxWidth
             << "source" << source;
             */

    // Done range
//    QLinearGradient doneGradient = getGradient(gradient);
    QRect doneRect(pxStart, htOffset, pxWidth, ht);
    pnt.fillRect(doneRect, gradient);
    m1->progressLabel->setPixmap(*(m1->progressPixmap));
}

void ProgressBar::updateDoneItem(bool isDone, int item, int items, QColor doneColor)
{
    QPainter pnt(m1->progressPixmap);
    int barWidth = m1->progressWidth;
    float itemWidth = (float)barWidth / items;
    int pxStart, pxWidth;
    pxWidth = qRound(itemWidth) + 1;
    pxStart = qRound(item * itemWidth);
    QLinearGradient doneGradient = getGradient(doneColor);
    QRect doneRect(pxStart, htOffset, pxWidth, ht);
    pnt.fillRect(doneRect, doneGradient);
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
