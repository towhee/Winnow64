#ifndef WIDGETCSS_H
#define WIDGETCSS_H

#include <QtWidgets>

class WidgetCSS
{
public:
    // Aggregate
    QString css();

    // QWidget
    int fg;                             // widget foreground shade (r = g = b = bg)
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
    int l3;
    int l5;
    int l10;
    int l15;
    int l20;
    int l30;
    int l40;
    int l50;
    int l60;

    // height factors
    QString h12;    // 1.2x font height
    QString h15;    // 1.5x font height
    QString h17;    // 1.7x font height
    QString h20;    // 2.0x font height

    int fontSize;
    int halfFontSize;
    int scrollBarWidth = 14;
    QColor textColor;     // = QColor(G::textShade,G::textShade,G::textShade);
    QColor disabledColor; // = QColor(G::backgroundShade+20,G::backgroundShade+20,G::backgroundShade+20);
    QColor borderColor;
    QColor widgetBackgroundColor;
    QColor progressBarBackgroundColor;

    QColor scrollBarHandleBackgroundColor;
    QColor selectionColor;
    QColor mouseOverColor;

    QString widget();
    QString mainWindow();
    QString dialog();
    QString doubleSpinBox();
    QString frame();
    QString graphicsView();
    QString statusBar();
    QString menuBar();
    QString menu();
    QString groupBox();
    QString label();
//    QString dockTitleBar();
    QString dockWidget();
    QString toolButton();
    QString tabWidget();
    QString stackedWidget();
    QString listView();
    QString listWidget();
    QString toolTip();
    QString treeWidget();
    QString treeView();
    QString tableView();
    QString headerView();
    QString scrollBar();
    QString pushButton();
    QString radioButton();
    QString comboBox();
    QString spinBox();
    QString textEdit();
    QString lineEdit();
    QString checkBox();
    QString progressBar();

signals:
    void fontSizeChange(const int &size);
};
#endif // WIDGETCSS_H
