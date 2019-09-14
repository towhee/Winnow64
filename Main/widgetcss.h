#ifndef WIDGETCSS_H
#define WIDGETCSS_H

#include <QtWidgets>

class WidgetCSS
{
public:
    // Aggregate
    QString css();

    // QWidget
    int bg;                             // widget background shade (r = g = b = bg)
    int mb;                             // menu background shade
    int fm;                             // frame shade relative to bg
    int g0;
    int g1;

    int fontSize;
    int scrollBarWidth = 14;
    QColor textColor;
    QColor widgetBackgroundColor;
    QColor scrollBarHandleBackgroundColor;
    QColor selectionColor;

    QString widget();
    QString mainWindow();
    QString frame();
    QString graphicsView();
    QString statusBar();
    QString menuBar();
    QString menu();
    QString groupBox();
    QString label();
    QString dockWidget();
    QString tabWidget();
    QString stackedWidget();
    QString listView();
    QString listWidget();
    QString treeWidget();
    QString treeView();
    QString tableView();
    QString headerView();
    QString scrollBar();
};
#endif // WIDGETCSS_H
