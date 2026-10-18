#include "Main/mainwindow.h"

void MW::helpPerformanceTips()
{
    if (G::isLogger) G::log("MW::helpPerformanceTips");
    QRect r = QRect(mapToGlobal(QPoint(0, 0)), size());
    new HtmlWindow("Winnow - Performance Tips",
                   ":/Docs/performancetipshelp.html",
                   QSize(600, 400), r, window());
}

void MW::helpFocusStackingTips()
{
    if (G::isLogger) G::log("MW::helpFocusStackingTips");
    QRect r = QRect(mapToGlobal(QPoint(0, 0)), size());
    new HtmlWindow("Winnow - Focus Stacking Tips",
                   ":/Docs/focusstackingtipshelp.html",
                   QSize(750, 600), r, window());
}

void MW::helpPictures()
{
/*
    Developed / Original / Camera, and the two controls (Show Edits for all images,
    Compare for one). Opened from the status-bar picture chip, View and Help.
*/
    if (G::isLogger) G::log("MW::helpPictures");
    QRect r = QRect(mapToGlobal(QPoint(0, 0)), size());
    new HtmlWindow("Winnow - Developed, Original and Camera",
                   ":/Docs/pictureshelp.html",
                   QSize(600, 620), r, window());
}
