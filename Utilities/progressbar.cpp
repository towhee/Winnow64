#include "progressbar.h"
#include "Main/mainwindow.h"
#include "Main/global.h"
#include <QDebug>

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

// Declare here as #include "mainwindow" causes errors if put in header
MW *mw1;

ProgressBar::ProgressBar(QWidget *parent) : QWidget(parent)
{
    mw1 = qobject_cast<MW*>(parent);
    htOffset = 9;               // the offset from the top of pnt to the progress bar
    ht = 8;                     // the height of the progress bar
}

void ProgressBar::clearProgress()
{
    counter = 0;
    QPainter pnt(mw1->progressPixmap);
    QLinearGradient bgGradient = getGradient(mw1->progressBgColor);
    QRect bgRect(0, htOffset, mw1->progressWidth, ht);
    pnt.fillRect(bgRect, bgGradient);
    mw1->progressLabel->setPixmap(*(mw1->progressPixmap));
}

void ProgressBar::updateProgress(int fromItem,
                                 int toItem,
                                 int items,
                                 QColor doneColor,
                                 QString source)
{
    // only show 1 in n to speed things up
    int n = 10;
    counter++;
//    int iterations = items / 10;
//    if(counter % n != 0 && counter > 1) return;

    QPainter pnt(mw1->progressPixmap);
    int barWidth = mw1->progressWidth;
    float itemWidth = (float)barWidth/items;
    int pxStart, pxWidth;

    // to and from can be mixed depending on direction of travel
    if(fromItem < toItem) {
        pxWidth = qRound((toItem - fromItem) * itemWidth) + 1;;
        pxStart = qRound(fromItem * itemWidth);
        if(pxStart + pxWidth > barWidth) pxWidth = barWidth - pxStart;
    }
    else {
        pxWidth = qRound((fromItem - toItem) * itemWidth) + 1;
        pxStart = qRound(toItem * itemWidth);
    }
//    pxWidth *= n;
    if(pxWidth < 2) pxWidth = 2;
    if(pxStart + pxWidth > barWidth) pxWidth = barWidth - pxStart;

//    qDebug() << "Target range from =" << fromItem
//             << "to" << toItem
//             << "itemWidth" << itemWidth
//             << "pxStart" << pxStart
//             << "pxWidth" << pxWidth
//             << "source" << source;

    // Done range
    QLinearGradient doneGradient = getGradient(doneColor);
    QRect doneRect(pxStart, htOffset, pxWidth, ht);
    pnt.fillRect(doneRect, doneGradient);
    mw1->progressLabel->setPixmap(*(mw1->progressPixmap));
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
