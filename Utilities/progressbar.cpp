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
    QPainter pnt(mw1->cachePixmap);
    QLinearGradient bgGradient = getGradient(mw1->progressBgColor);
    QRect bgRect(0, htOffset, mw1->cacheStatusWidth, ht);
    pnt.fillRect(bgRect, bgGradient);
    mw1->cacheLabel->setPixmap(*(mw1->cachePixmap));
}

void ProgressBar::updateProgress(int fromItem, int toItem, int items, QColor doneColor)
{
    QPainter pnt(mw1->cachePixmap);
    int barWidth = mw1->cacheStatusWidth;
    int itemWidth = qRound((float)barWidth/items);
    if (itemWidth == 0) itemWidth++;

    // Done range
    QLinearGradient doneGradient = getGradient(doneColor);
    QRect doneRect(fromItem * itemWidth, htOffset, toItem * itemWidth, ht);
    pnt.fillRect(doneRect, doneGradient);
    mw1->cacheLabel->setPixmap(*(mw1->cachePixmap));
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
