#ifndef WIDGETCSS_H
#define WIDGETCSS_H

#include <QtWidgets>
#include "Main/global.h"

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

    // darker than background
    int d5;
    int d10;
    int d15;
    int d20;

    // lighter than background
    int l5;
    int l10;
    int l15;
    int l20;
    int l30;
    int l40;

    int fontSize;
    int scrollBarWidth = 14;
    QColor textColor = QColor(190,190,190);
    QColor widgetBackgroundColor ;
    QColor scrollBarHandleBackgroundColor = QColor(90,130,100);
    QColor selectionColor = QColor(68,95,118);

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
    QString pushButton();
    QString comboBox();
    QString spinBox();
    QString textEdit();
    QString lineEdit();
};
#endif // WIDGETCSS_H