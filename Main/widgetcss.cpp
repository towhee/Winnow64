#include "Main/widgetcss.h"

QString WidgetCSS::css()
{
    fg = G::textShade;
    bg = widgetBackgroundColor.red();   // desaturate
    mb = bg + 15;
    fm = bg + 35;
    g0 = bg - 10;
    if (g0 < 0) g0 = 0;
    g1 = g0 + 30;

    // darker than background
    d5  = bg - 5;
    d10 = bg - 10;
    d15 = bg - 15;
    d20 = bg - 20;
    if (d15 < 0) d15 = 0;
    if (d20 < 0) d20 = 0;

    // lighter than background
    l5  = bg + 5;
    l10 = bg + 10;
    l15 = bg + 15;
    l20 = bg + 20;
    l30 = bg + 30;
    l40 = bg + 40;
    l50 = bg + 50;
    l60 = bg + 60;

    textColor = QColor(fg,fg,fg);
    disabledColor = QColor(l20,l20,l20);
    scrollBarHandleBackgroundColor = QColor(90,130,100);
    selectionColor = QColor(68,95,118);
    borderColor = QColor(l40,l40,l40);

    // heights (mostly used for rows in TreeView etc)
    h12 = QString::number(fontSize * 1.2 * G::ptToPx);
    h15 = QString::number(fontSize * 1.5 * G::ptToPx);
    h17 = QString::number(fontSize * 1.7 * G::ptToPx);
    h20 = QString::number(fontSize * 2.0 * G::ptToPx);

    halfFontSize = fontSize / 2;

    return
            widget() +
            mainWindow() +
            menu() +
            menuBar() +
            checkBox() +
            comboBox() +
//            dockTitleBar() +
            dockWidget() +
            doubleSpinBox() +
            graphicsView() +
            groupBox() +
            headerView() +
            label() +
            lineEdit() +
            listView() +
            listWidget() +
            progressBar() +
            pushButton() +
            radioButton() +
            scrollBar() +
            spinBox() +
            stackedWidget() +
            statusBar() +
            tableView() +
            tabWidget() +
            textEdit() +
            toolButton() +
            toolTip() +
            treeView() +
            treeWidget()
            ;
}

QString WidgetCSS::widget()
{
    return
    "QWidget {"
       "font-size:" + QString::number(fontSize) + "pt;"
       "background-color:" + widgetBackgroundColor.name() + ";"
       "color:" + textColor.name() + ";"
       "border-width: 0px;"
    "}"
    "QWidget:disabled {"
//    "color:" + disabledColor.name() + ";"
    "color:" + QColor(mb,mb,mb).name() + ";"
    "}"
   ;
}

QString WidgetCSS::mainWindow()
{
    return
    "QMainWindow::separator {"
        "background: " + QColor(l5,l5,l5).name() + ";"
        "width: 4px;"
        "height: 4px;"
    "}"

    "QMainWindow::separator:hover {"
        "background: " + QColor(l40,l40,l40).name() + ";"
    "}";
}

QString WidgetCSS::dialog()
{
    return
    "QDialog {"
        "background: " + QColor(l10,l10,l10).name() + ";"
    "}";
}

QString WidgetCSS::frame()
{
    /*HLine and VLine*/
    return
    "QFrame[frameShape=""4""],"
    "QFrame[frameShape=""5""]"
    "{"
        "border: none;"
        "background: " + QColor(fm,fm,fm).name() + ";"
    "}";
}

QString WidgetCSS::graphicsView()
{
    return
    "QGraphicsView {"
        "border: none;"
   "}";
}

QString WidgetCSS::statusBar()
{
    return
    "StatusBar::QLabel {"
        "color:" + textColor.name() + ";"
    "}"
    "QStatusbar::item {"
        "border: none;"
    "}";
}

QString WidgetCSS::menuBar()
{
    return
    "QMenuBar {"
       "border: 0px solid " + QColor(mb,mb,mb).name() + ";"
    "}"
    "QMenuBar::item {"
        "spacing: 2px;"
        "padding: 6px 6px;"
        "background: transparent;"
        "border-radius: 4px;"
    "}"
    "QMenuBar::item:selected {"
        "background-color: " + selectionColor.name() + ";"
    "}"

    "QMenuBar::item:pressed {"
    "background-color: " + selectionColor.name() + ";"
    "}";
}

QString WidgetCSS::menu()
{
    return
    "QMenu {"
    "background-color: " + QColor(mb,mb,mb).name() + ";"
    "border: 1px solid gray;"
    "}"

    "QMenu::item {"
        "color: white;"
        "background-color: transparent;"
    "}"

    "QMenu::item:selected {"
        "background-color: " + selectionColor.name() + ";"
    "}"

    "QMenu::item:disabled {"
        "color: gray;"
    "}";
}

QString WidgetCSS::groupBox()
{
    return
    "QGroupBox {"
        "border: 1px solid " + QColor(l60,l60,l60).name() + ";"
        "border-radius: 5px;"
        "margin-top: 0.5em;"/* leave space at the top for the title */
    "}"

    "QGroupBox::title {"
        "subcontrol-origin: margin;"
        "subcontrol-position: top left;"
        "margin-top: " + QString::number(halfFontSize) + ";"
        "right: -20px;"
        "top: -" + QString::number(halfFontSize) + "px;"
    "}";
}

QString WidgetCSS::label()
{
    return
    "QLabel {"
        "border: none;"
    "}"

    "QLabel:disabled {"
        "color:" + disabledColor.name() + ";"
    "}"
    ;
}

QString WidgetCSS::toolButton()
{
    return
    "QToolButton {"
        "background:transparent;"
        "border:none;"
    "}"
    "QToolButton:hover {"
//    "background:green;"
    "background:" + QColor(l30,l30,l30).name() + ";"
    "border:none;"
    "}"
    ;
}

QString WidgetCSS::toolTip()
{
    return
    "QToolTip {"
        "background:white;"
        "color:black;"
    "}"
    ;
}

QString WidgetCSS::dockWidget()
{
    return
//    "QDockWidget > QWidget {"
    "QDockWidget {"
        "color: " + textColor.name() + ";"
        "margin: 0px;"
        "padding: 0px;"
        "spacing: 0px;"
        "border: 5px solid green;"
//        "border-color: green;"
//        "border-color: " + QColor(l40,l40,l40).name() + ";"
    "}"

    "QDockWidget::title {"
        "background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,"
            "stop: 0 " + QColor(g1,g1,g1).name() + ", "
            "stop: 1 " + QColor(g0,g0,g0).name() + ");"
        "padding-left: -2px;"
        "padding-bottom: 2px;"
//        "border: 1px;"
//        "border-color: " + textColor.name() + ";"
    "}"

    "QTabWidget::pane {"                      /* The tab widget frame */
//        "border: 1px solid " + QColor(l40,l40,l40).name() + ";"    /* not working in main window */
        "position: absolute;"
        "top: -0.5em;"
        "border-radius: 5px;"
    "}";
}

// Not working
//QString WidgetCSS::dockTitleBar()
//{
//    return
//    "QWidget#DockTitleBar {"
//        "background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,"
//        "stop: 0 " + QColor(bg,bg,bg).name() + ", "
//        "stop: 1 " + QColor(g0,g0,g0).name() + ");"
//        "padding-left: 2px;"
//        "padding-bottom: 2px;"
//        "font-size:" + G::fontSize + "pt;"
//    "}";
//}

QString WidgetCSS::tabWidget()
{
    return
    "QTabWidget::tab-bar {"
//        "left: 0;"
//        "alignment: left;"
/*        "border: 1px solid red;"                      nada */
    "}"

    "QTabBar::tab {"
        "color: silver;"
        "background-color: " + QColor(l20,l20,l20).name() + ";"
        "border: 1px solid " + QColor(mb,mb,mb).name() + ";"
        "padding: 4px 4px;"
    "}"

    /*QTabBar:selected {                    nada
        border: red;
    }*/

    "QTabBar::tab:selected {"
        "color: cadetblue;"
        "border-bottom: 0px;"
        "background-color: " + QColor(bg,bg,bg).name() + ";"
   " }"

    "QTabBar::tab:disabled {"
        "color:" + disabledColor.name() + ";"
    "}";
}

QString WidgetCSS::stackedWidget()
{
    return
    "QStackedWidget {"
        "border: 1px solid " + QColor(fm,fm,fm).name() + ";"
        /*border: 1px solid rgb(95,95,95);*/
        "border-radius: 5px;"
    "}";
}

QString WidgetCSS::listView()
{
    return
    "QListView {"
        "border: 1px solid " + QColor(bg,bg,bg).name() + ";"
    "}";

}

QString WidgetCSS::listWidget()
{
    return
    "QListWidget {"
        /*alternate-background-color: rgb(90,90,90);*/
        "gridline-color: " + QColor(d10,d10,d10).name() + ";"
        "color: " + textColor.name() + ";"
        "selection-background-color: " + selectionColor.name() + ";"
        "border: 1px solid " + QColor(l10,l10,l10).name() + ";"
     "}";
}

QString WidgetCSS::treeWidget()
{
    return
    "QTreeWidget {"
//        "background-color: " + QColor(bg,bg,bg).name() + ";"
        "alternate-background-color: " + QColor(l5,l5,l5).name() + ";"
        "border: 2px solid " + QColor(l10,l10,l10).name() + ";"
        "color: lightgray;"
    "}"

    "QTreeWidget::item:disabled {"
        "color:" + disabledColor.name() + ";"
    "}"

    "QTreeWidget::item {"
        "height: 24px;"
        "margin-left: -1px;"  // aligns edit box with cell contents
    "}"

    "QTreeWidget::indicator:disabled {"
        "image: url(:/images/checkbox_disabled.png);"
    "}"

    ;
}

QString WidgetCSS::treeView()
{
    return
    "QTreeView {"
//        "background-color: " + QColor(bg,bg,bg).name() + ";"
        "alternate-background-color: " + QColor(l5,l5,l5).name() + ";"
        "color: " + textColor.name() + ";"
        "selection-background-color: " + selectionColor.name() + ";"
        "selection-color: " + textColor.name() + ";"
        "border: 1px solid " + QColor(mb,mb,mb).name() + ";"
    "}"

    "QTreeView::branch:has-children:!has-siblings:closed,"
    "QTreeView::branch:closed:has-children:has-siblings {"
        "border-image: none;"
        "image: url(:/images/branch-closed-winnow.png);"
//        "margin-bottom: 2px;"
    "}"

    "QTreeView::branch:open:has-children:!has-siblings,"
    "QTreeView::branch:open:has-children:has-siblings  {"
        "border-image: none;"
        "image: url(:/images/branch-open-winnow.png);"
//        "margin-bottom: 2px;"
    "}"

    "QTreeView::item {"
//        "height: " + h17 + "px;"      // this works but delegates and defaults working for now
    "}";
}

QString WidgetCSS::tableView()
{
    return
    "QTableView {"
        "alternate-background-color: " + QColor(l5,l5,l5).name() + ";"
        "gridline-color: " + QColor(l10,l10,l10).name() + ";"
        "color: " + textColor.name() + ";"
        "selection-color: " + textColor.name() + ";"
        "selection-background-color: " + selectionColor.name() + ";"
        "border: none;"
    "}";
}

QString WidgetCSS::headerView()
{
    return
    "QHeaderView::section {"
        "background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,"
            "stop: 0 " + QColor(g1,g1,g1).name() + ", "
            "stop: 1 " + QColor(g0,g0,g0).name() + ");"
        "border-style: solid;"
        "border: 1px solid " + QColor(bg,bg,bg).name() + ";"
        "border-bottom: 1px solid " + QColor(bg,bg,bg).name() + ";"
        "color:" + textColor.name() + ";"
        "max-height: 24px;"
    "}";
}

QString WidgetCSS::scrollBar()
{
    return
    "QScrollBar:vertical {"
        "border: 1px solid " + QColor(l10,l10,l10).name() + ";"
        "background: " + QColor(l10,l10,l10).name() + ";"
        "margin-top: 0px;"
        "margin-bottom: 0px;"
        "width: " + QString::number(scrollBarWidth) + "px;"       /* also set for QScrollBar:horizontal and global.cpp */
    "}"

    "QScrollBar::handle:vertical {"
        "border: 1px solid " + QColor(l40,l40,l40).name() + ";"
//        "border-right: 0px solid " + QColor(l10,l10,l10).name() + ";"
        "min-height: 20px;"
        "border-radius: 2px;"  /*not working, border=0, maybe if pad*/
    "}"

    "QScrollBar::handle:hover {"
        "background-color: " + scrollBarHandleBackgroundColor.name() + ";"
    "}"

    "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical{"
        "background-color: " + QColor(bg,bg,bg).name() + ";"
    "}"

    "QScrollBar::add-line:vertical {"
        "border: 1px solid " + QColor(l10,l10,l10).name() + ";"
        "height: 1px;"
        "subcontrol-position: bottom;"
        "subcontrol-origin: margin;"
    "}"

    "QScrollBar::sub-line:vertical {"
        "border: 1px solid " + QColor(l10,l10,l10).name() + ";"
        "height: 1px;"
        "subcontrol-position: top;"
        "subcontrol-origin: margin;"
    "}"

    "QScrollBar::up-arrow:vertical, QScrollBar::down-arrow:vertical {"
        "width: 1px;"
        "height: 1px;"
        "background-color: " + QColor(d10,d10,d10).name() + ";"
    "}"

    "QScrollBar:horizontal {"
        "border: 1px solid " + QColor(l10,l10,l10).name() + ";"
        "background: " + QColor(l10,l10,l10).name() + ";"
        "margin-left: 0px;"
        "margin-right: 0px;"
        "height: " + QString::number(scrollBarWidth) + "px;"
    "}"

    "QScrollBar::handle:horizontal {"
        "border: 1px solid " + QColor(l40,l40,l40).name() + ";"
        "min-width: 20px;"
    "}"

    "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal{"
        "background-color: " + QColor(bg,bg,bg).name() + ";"
    "}"

    "QScrollBar::add-line:horizontal {"
        "border: 1px solid " + QColor(l10,l10,l10).name() + ";"
        "width: 1px;"
        "subcontrol-position: right;"
        "subcontrol-origin: margin;"
    "}"

    "QScrollBar::sub-line:horizontal {"
        "border: 1px solid " + QColor(l10,l10,l10).name() + ";"
        "width: 1px;"
        "subcontrol-position: left;"
        "subcontrol-origin: margin;"
    "}"

    "QScrollBar::up-arrow:horizontal, QScrollBar::down-arrow:horizontal {"
        "width: 0px;"
        "height: 0px;"
        "background: " + QColor(d10,d10,d10).name() + ";"
    "}";
}

QString WidgetCSS::radioButton()
{
    return
    "QRadioButton:checked {"
        "color: cadetblue;"
    "}"
    "QRadioButton:unchecked {"
        "color:" + textColor.name() + ";"
    "}";
}

QString WidgetCSS::pushButton()
{
    return
    /*PushButton must be before ComboBox*/
    "QPushButton {"
        "background-color: " + QColor(d10,d10,d10).name() + ";"
        "border-width: 1px;"
        "border-style: solid;"
        "border-color: " + QColor(l10,l10,l10).name() + ";"
        "border-radius: 2px;"
        "padding-left: 5px;"
        "padding-right: 5px;"
        "padding-top: 3px;"
        "padding-bottom: 3px;"
        "min-width: 100px;"
//        "min-height: " + h12 + "px;"  // screws up height in PlusMinusEditor
    "}"

    "QPushButton:default {"
        "border: 1px solid cadetblue;"
    "}"

    "QPushButton:pressed {"
        "background-color: gray;"
    "}"

    "QPushButton:hover {"
        "border-color: white;"
        "background-color: " + selectionColor.name() + ";"
    "}"

    "QPushButton:flat {"
        /* no border for a flat push button by default */
//        "border: 1px solid gray;"
//        "border-width: 1px;"
//        "border-style: solid;"
//        "border-color: " + QColor(l5,l5,l5).name() + ";"
    "}"

    "QPushButton:disabled {"
        "background-color: " + QColor(d5,d5,d5).name() + ";"
        "color:" + disabledColor.name() + ";"
    "}";
}

QString WidgetCSS::comboBox()
{
    return
    "QComboBox {"
        "background-color: " + QColor(d10,d10,d10).name() + ";"
        "border-width: 1px;"
        "border-style: solid;"
        "border-color: " + borderColor.name() + ";"
        "border-radius: 2px;"
        "padding: 0px 10px 1px 8px;"  /*text  top, right, bottom, left*/
        "min-width: 6em;"
    "}"

    "QComboBox:hover, QComboBox:focus {"
        "border-color: silver;"
    "}"

    "QComboBox:disabled {"
        "color:" + disabledColor.name() + ";"
        "border-color:" + disabledColor.name() + ";"
        "background-color: " + QColor(d5,d5,d5).name() + ";"
    "}"

    // Arrow button that activates dropdown list
    "QComboBox::drop-down {"
        "subcontrol-origin: padding;"
        "subcontrol-position: top right;"
        "width: 18px;"
//        "border-left-width: 1px;"
//        "border-left-color: darkgray;"
//        "border-left-style: solid;" /* just a single line */
        "border-top-right-radius: 5px;" /* same radius as the QComboBox */
        "border-bottom-right-radius: 5px;"
    "}"

    "QComboBox::drop-down:disabled {"
        "border-color:" + disabledColor.name() + ";"
    "}"

    "QComboBox::down-arrow {"
        "image: url(:/images/down-arrow3.png);"
        "border-color: " + borderColor.name() + ";" //nada
    "}"

    "QComboBox::down-arrow:disabled {"
         "image: url(:/images/no_image.png);"
    "}"

    "QComboBox::down-arrow:on {" /* shift the arrow when popup is open */
        "top: 1px;"
        "left: 1px;"
    "}"

    // Dropdown list
    "QComboBox QAbstractItemView::item {"
        "height: 20px;"
    "}"
    ;
}

QString WidgetCSS::spinBox()
{
    /* NOTE - the spinbox style is "subclassed" in ZoomDlg, so any changes here
    should also be made there ...  */
    return
    "QSpinBox {"
        "background-color: " + QColor(d10,d10,d10).name() + ";"
//        "border: 1px solid gray;"
        "border-width: 1px;"
        "border-style: solid;"
        "border-color: " + borderColor.name() + ";"
        "selection-background-color: darkgray;"
        "border-radius: 2px;"
        "padding-left: 4px;"
    "}"

    "QSpinBox:hover, QSpinBox:focus {"
        "border-color: silver;"
    "}"

    "QSpinBox:disabled {"
        "color:" + disabledColor.name() + ";"
        "background-color: " + QColor(bg,bg,bg).name() + ";"
    "}"

    "QSpinBox::up-button, QSpinBox::down-button  {"
        "width: 0px;"
        "border-width: 0px;"
    "}";
}

QString WidgetCSS::doubleSpinBox()
{
    /* NOTE - the spinbox style is "subclassed" in ZoomDlg, so any changes here
    should also be made there ...  */
    return
    "QDoubleSpinBox {"
    "background-color: " + QColor(d10,d10,d10).name() + ";"
//    "border: 1px solid gray;"
    "border-width: 1px;"
    "border-style: solid;"
    "border-color: " + borderColor.name() + ";"
    "selection-background-color: darkgray;"
    "border-radius: 2px;"
    "padding-left: 4px;"
    "}"

    "QDoubleSpinBox:hover, QDoubleSpinBox:focus {"
    "border-color: silver;"
    "}"

    "QDoubleSpinBox:disabled {"
    "color:" + disabledColor.name() + ";"
    "background-color: " + QColor(bg,bg,bg).name() + ";"
    "}"

    "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button  {"
    "width: 0px;"
    "border-width: 0px;"
    "}";
}

QString WidgetCSS::textEdit()
{
    return
    "QTextBrowser, QTextEdit {"
        "background-color: " + QColor(d10,d10,d10).name() + ";"
//        "border: 1px solid red;"
        "border: none;"
    "}";
}

QString WidgetCSS::lineEdit()
{
    return
    "QLineEdit {"
        "background-color: " + QColor(d10,d10,d10).name() + ";"
//        "border: 1px solid gray;"
        "border-width: 1px;"
        "border-style: solid;"
        "border-color: " + borderColor.name() + ";"
        "margin-left: 5px;"   /*Not sure why, but this is required*/
//        "selection-background-color: darkgray;"
        "padding-top: 1px;"
        "padding-bottom: 1px;"
        "padding-left: 4px;"
        "border-radius: 2px;"
    "}"

    "QLineEdit:hover, QLineEdit:focus {"
//        "border-color: red;"
    "}"

    "QLineEdit:disabled {"
        "color:" + disabledColor.name() + ";"
        "border-color:" + disabledColor.name() + ";"
    "}";
}

QString WidgetCSS::progressBar()
{
    return
    "QProgressBar {"
        "background-color: " + QColor(bg,bg,bg).name() + ";"
    "}"

    "QProgressBar::chunk {"
        "background-color: cadetblue;"
    "}";
}

QString WidgetCSS::checkBox()
{
//    return "";
    return
    "QCheckBox {"
//        "color: blue;"
    "}";
//    "QCheckBox::indicator {"
//        "background-color: " + textColor.name() + ";"
////        "background-color: " + QColor(l30,l30,l30).name() + ";"
////        "image: url(:/images/checkmark.png);"
//    "}"
//    ;
//    "QCheckBox::indicator:checked {"
//        "background-color: " + QColor(l30,l30,l30).name() + ";"
//    "}"
//    "QCheckBox::indicator:checked:disabled {"
//        "background-color: " + QColor(l30,l30,l30).name() + ";"
////        "image: url(:/images/checkmark.png);"
////        "border: 1px solid red;"
//    "}"
////        "QCheckBox::indicator:indeterminate {"
////    //        "image: url(:/images/checkmark.png);"
////            "border: 3px solid blue;"
////        "}"
//     ;
}
