#include "Main/mainwindow.h"

void MW::helpPerformanceTips()
{
    if (G::isLogger) G::log("MW::helpPerformanceTips");
    QRect r = QRect(mapToGlobal(geometry().topLeft()), geometry().size());
    new HtmlWindow("Winnow - Performance Tips",
                   ":/Docs/performancetipshelp.html",
                   QSize(600, 400), r, window());
}

void MW::helpFocusStackingTips()
{
    if (G::isLogger) G::log("MW::helpFocusStackingTips");
    QRect r = QRect(mapToGlobal(geometry().topLeft()), geometry().size());
    new HtmlWindow("Winnow - Focus Stacking Tips",
                   ":/Docs/focusstackingtipshelp.html",
                   QSize(750, 600), r, window());
}
