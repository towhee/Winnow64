#include "Main/widgetcss.h"
#include "Main/global.h"

QString WidgetCSS::css()
{
    fg = G::textShade;
    bg = G::backgroundShade;
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
    l3  = bg + 3;
    l5  = bg + 5;
    l10 = bg + 10;
    l15 = bg + 15;
    l20 = bg + 20;
    l30 = bg + 30;
    l40 = bg + 40;
    l50 = bg + 50;
    l60 = bg + 60;

    textColor = QColor(fg,fg,fg);
    disabledColor = QColor(l40,l40,l40);
    G::disabledColor = QColor(l40,l40,l40);
    G::tabWidgetBorderColor = QColor(l60,l60,l60);
    G::pushButtonBackgroundColor = QColor(d10,d10,d10);
    borderColor = QColor(l40,l40,l40);
    G::borderColor = borderColor;

    G::scrollBarHandleBackgroundColor = QColor(bg,l3,bg);
    selectionColor = G::selectionColor;
    mouseOverColor = G::mouseOverColor;  // not being used, matches what happens in treeview on windows
    progressBarBackgroundColor = QColor(d10,d10,d10);

    // heights (mostly used for rows in TreeView etc)
    h12 = QString::number(fontSize * 1.2 * G::ptToPx);
    h15 = QString::number(fontSize * 1.5 * G::ptToPx);
    h17 = QString::number(fontSize * 1.7 * G::ptToPx);
    h20 = QString::number(fontSize * 2.0 * G::ptToPx);

    halfFontSize = fontSize / 2;

    // border radius
    brInteractive  = "6px";
    brContainer    = "8px";
    brLargeObjects = "10px";

    QString blank = "";

    // do not include frame(), it is inconsistent and screws up stuff, depending on order
    return
            widget() +
            mainWindow() +
            menu() +
            menuBar() +
            checkBox() +
            comboBox() +
            dialog() +
            dockWidget() +
            dockTabBar() +
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
            treeView() +
            #ifdef Q_OS_MAC
            toolTip() +
            #endif

           // dockTitleBar() +      // not working
           // treeWidget() +        // not working

            blank                   // allows comment out any above
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
        "background: " + QColor(bg,bg,bg).name() + ";"
    "}";
}

QString WidgetCSS::frame()
{
    return
    "QFrame[frameShape=""4""],"
    "QFrame[frameShape=""5""]"
    "{"
        "border: none;"
        "color: " + QColor(fm,fm,fm).name() + ";"
        "background: " + QColor(fm,fm,fm).name() + ";"
        // cannot assign line color here: see https://stackoverflow.com/questions/14581498/qt-stylesheet-for-hline-vline-color
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
    "QStatusBar::item {"
        "border: none;"
    "}";
}

QString WidgetCSS::menuBar()
{
    // On Windows the menu bar sits directly above the dock tab bar with no
    // visual separation. Add a bottom border (same frame color as
    // DockTitleBar, fm = backgroundShade + 35) to divide the menu from the
    // docks. macOS keeps its native menu bar, so no border is needed there.
#ifdef Q_OS_WIN
    QString menuBarRule =
    "QMenuBar {"
       "border: 0px solid " + QColor(mb,mb,mb).name() + ";"
       "border-bottom: 1px solid " + QColor(mb,mb,mb).name() + ";"
    "}";
#else
    QString menuBarRule =
    "QMenuBar {"
       "border: 0px solid " + QColor(mb,mb,mb).name() + ";"
    "}";
#endif
    return
    menuBarRule +
    "QMenuBar::item {"
        "spacing: 2px;"
        "padding: 6px 6px;"
        "background: transparent;"
        // "border-radius: " + brLargeObjects + ";"
    "}"
    "QMenuBar::item:selected {"
        "background-color: " + selectionColor.name() + ";"
    "}"

    "QMenuBar::item:pressed {"
        "background-color: " + selectionColor.name() + ";"
    "}"

    "QMenuBar::item:disabled {"
        "color:" + disabledColor.name() + ";"
    "}"
    ;
}

QString WidgetCSS::menu()
{
    return
    "QMenu {"
        "background-color: " + QColor(mb,mb,mb).name() + ";"
        "border: 1px solid gray;"
        "border-radius: " + brLargeObjects + ";"
    "}"

    "QMenu::item {"
        //"color: white;"     // nada
        "background-color: transparent;"
    "}"

    "QMenu::item:selected {"
        "background-color: " + selectionColor.name() + ";"
    "}"

    "QMenu::item:disabled {"
        "color:" + disabledColor.name() + ";"
    "}"
    ;
}

QString WidgetCSS::groupBox()
{
    return
    "QGroupBox {"
        "border: 1px solid " + QColor(l60,l60,l60).name() + ";"
        "border-radius: " + brContainer + ";"
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

    "QLabel#statusLabel {"
        "font-family: Menlo, Consolas, monospace;"
    "}"

    // Cyan accent for dock widget titles - intentionally not theme-tied.
    "DockTitleBar > QLabel {"
        "border: none;"
        "background: transparent;"
        "padding-left: 4px;"
        "color: #6CC1E8;"
    "}"

    // Status dots need a larger glyph on Windows to be readable.
    #ifdef Q_OS_WIN
    "QLabel#MetadataCacheStatus, QLabel#ImageCacheStatus {"
        "font-size: 24px;"
    "}"
    #endif
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
        "opacity: 200;"         // nada windows
        "color: " + QColor(fg,fg,fg).name() + ";"   // nada windows
        "background-color: " + QColor(d5,d5,d5).name() + ";"
        "border-width: 1px;"
        "border-style: solid;"
        "border-radius: " + brLargeObjects + ";"
        "border-color: " + QColor(l10,l10,l10).name() + ";"
        "margin: 2px;"
        "font-size:" + QString::number(fontSize) + "px;"
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
    "}"

    "QDockWidget::title {"
        "text-align: left center;"
        "background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,"
            "stop: 0 " + QColor(g1,g1,g1).name() + ", "
            "stop: 1 " + QColor(g0,g0,g0).name() + ");"
        "padding: 8px;"
    "}"
     ;
}

QString WidgetCSS::dockTabBar()
{
    return
    "QMainWindow > QTabBar::tab {"
        // "color: " + QColor(fg-40,fg-40,fg-40).name() + ";"
        // "background-color: " + QColor(l20,l20,l20).name() + ";"
        "border: 1px solid " + QColor(mb,mb,mb).name() + ";"
        // "border-bottom: none;"
        // "border-radius: " + brInteractive + ";"
        "border-top-left-radius: " + brInteractive + ";"
        "border-top-right-radius: " + brInteractive + ";"
        // "border-bottom-left-radius: 0px;"
        // "border-bottom-right-radius: 0px;"

        // "padding: 3px 10px;"
        // "margin-right: 2px;"
     "}"

    "QMainWindow > QTabBar::tab:selected {"
        "color: " + G::textColor.name() + ";"
        "background-color: " + G::selectionColor.name() + ";"
     "}"

     "QMainWindow > QTabBar::tab:!selected {"
        "margin-top: 2px;"   // sit lower so selected tab pops
     "}"
        ;
}


/* Not working
QString WidgetCSS::dockTitleBar()
{
   return
   "QWidget#DockTitleBar {"
       "background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,"
       "stop: 0 " + QColor(bg,bg,bg).name() + ", "
       "stop: 1 " + QColor(g0,g0,g0).name() + ");"
       "padding-left: 2px;"
       "padding-bottom: 2px;"
       "font-size:" + G::fontSize + "pt;"
   "}";
}
*/

QString WidgetCSS::tabWidget()
{
    return
    "QTabWidget::pane {"
        "margin-top: -1px;"
    "}"

    "QTabWidget::tab-bar {"
        "alignment: left;"
//        "qproperty-drawBase: 1;"        // nada
    "}"

    "QTabBar::tab {"
        "color: " + QColor(fg-40,fg-40,fg-40).name() + ";"
        "background-color: " + QColor(l20,l20,l20).name() + ";"
//        "background-color: " + QColor(l10,l10,l10).name() + ";"
        "border: 1px solid " + QColor(mb,mb,mb).name() + ";"
        "padding-top: 2px;"
        "padding-bottom: 3px;"
        "padding-left: 5px;"
        "padding-right: 5px;"
    "}"

    "QTabBar::tab:selected {"
        "color:" + G::textColor.name() + ";"
        "border-bottom: 0px;"
        "background-color: " + G::selectionColor.name() + ";"
//        "background-color: " + QColor(bg,bg,bg).name() + ";"
    " }"

    "QTabBar::tab:disabled {"
        "color:" + disabledColor.name() + ";"
    "}"
    ;
}

QString WidgetCSS::stackedWidget()
{
    return
    "QStackedWidget {"
        "border: 1px solid " + QColor(fm,fm,fm).name() + ";"
        /*border: 1px solid rgb(95,95,95);*/
        "border-radius: " + brContainer + ";"
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
        "background-color: " + QColor(d10,d10,d10).name() + ";"
        "gridline-color: " + QColor(bg,bg,bg).name() + ";"
        "color: " + textColor.name() + ";"
        "selection-background-color: " + selectionColor.name() + ";"
        "border: 1px solid " + QColor(l10,l10,l10).name() + ";"
     "}";
}

QString WidgetCSS::treeWidget()
{
    return "";
    return
    "QTreeWidget {"
//        "background-color: " + QColor(bg,bg,bg).name() + ";"
        "alternate-background-color: " + QColor(l5,l5,l5).name() + ";"
        "selection-background-color: " + selectionColor.name() + ";"
        "border: 2px solid " + QColor(l10,l10,l10).name() + ";"
        "color: lightgray;"
    "}"

    "QTreeWidget::item {"
        "height: 24px;"
//        "margin-left: -1px;"  // aligns edit box with cell contents
    "}"

    "QTreeWidget::item:disabled {"
        "color:" + disabledColor.name() + ";"
    "}"

//    "QTreeWidget::item:focus {"
//        "background-color: green;"/* + disabledColor.name() + ";"*/
//        "margin-left: 15px;"
//    "}"

    "QTreeWidget::indicator {"
        "width: 15px;"
        "height: 15px;"
    "}"

    "QTreeWidget::indicator:disabled {"
        "image: url(:/images/checkbox_disabled.png);"
    "}"

    "QTreeWidget::indicator:checked {"
        "image: url(:/images/checkbox_checked_blue.png);"
        "margin-left: -5px;"
        "padding-left: 5px;"
    "}"

    "QLineEdit:focus {"
//        "background-color:" + G::selectionColor.name() + ";"
    "}"
    ;
}

QString WidgetCSS::treeView()
{
    return
    "QTreeView, QTreeWidget {"
        "alternate-background-color: " + QColor(l5,l5,l5).name() + ";"
        "color: " + textColor.name() + ";"
        "border: 1px solid " + QColor(mb,mb,mb).name() + ";"
    "}"

    "QTreeView::branch:has-children:!has-siblings:closed,"
    "QTreeView::branch:closed:has-children:has-siblings {"
        "border-image: none;"
        "image: url(:/images/branch-closed-winnow.png);"
    "}"

    "QTreeView::branch:open:has-children:!has-siblings,"
    "QTreeView::branch:open:has-children:has-siblings  {"
        "border-image: none;"
        "image: url(:/images/branch-open-winnow.png);"
    "}"

    "QTreeView::item {"
        //"height: " + h17 + "px;"      // this works but delegates and defaults working for now
    "}"

    "QTreeView::item:selected {"
        // interferes with FSTree overLimitColor formatting
        // "color: " + textColor.name() + ";"
        "background-color: " + selectionColor.name() + ";"
    "}"

    "QTreeView::item:selected:!active {"
        // interferes with FSTree overLimitColor formatting
        // "color: " + textColor.name() + ";"
        "background: " + selectionColor.name() + ";"
    "}"

#ifdef Q_OS_WIN
    // Match the G::css QCheckBox style for checkable tree items (eg Filters panel).
    // Without these the indicators fall back to the native Windows checkbox.
    "QTreeView::indicator {"
        "width: 15px;"
        "height: 15px;"
    "}"

    "QTreeView::indicator:unchecked {"
        "image: url(:/images/checkbox_unchecked_blue.png);"
    "}"

    "QTreeView::indicator:checked {"
        "image: url(:/images/checkbox_checked_blue.png);"
    "}"

    "QTreeView::indicator:disabled {"
        "image: url(:/images/checkbox_disabled.png);"
    "}"
#endif
    ;
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
        "border-radius: " + brInteractive + ";"
        "min-height: 20px;"
    "}"

    "QScrollBar::handle:hover {"
    "background-color: " + G::scrollBarHandleBackgroundColor.name() + ";"
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
        "border-radius: " + brInteractive + ";"
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
        "border-radius: " + brInteractive + ";"
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
    "}"

    "QDialogButtonBox QPushButton {"
    "min-height: 24px;"
    "min-width: 75px;"
    "}";}

QString WidgetCSS::comboBox()
{
    return
    "QComboBox {"
        "background-color: " + QColor(d10,d10,d10).name() + ";"
        "border-width: 1px;"
        "border-style: solid;"
        "border-color: " + borderColor.name() + ";"
        "border-radius: " + brInteractive + ";"
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
        "border-radius: " + brInteractive + ";"   /* same radius as the QComboBox */
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
        "selection-background-color:" + G::selectionColor.name() + ";"
        "border-radius: " + brInteractive + ";"
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
    "selection-background-color:" + G::selectionColor.name() + ";"
    "border-radius: " + brInteractive + ";"
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
//        "background: green;"
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
        "border: 1px solid gray;"
        "border-radius: " + brInteractive + ";"
        "padding-left: 4px;"
        "padding-right: 4px;"
        "selection-background-color:" + G::selectionColor.name() + ";"
    "}"

//    "QLineEdit:focus {"
//        "background-color: transparent;"
//    "}"

    "QLineEdit:hover {"
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
        "border: 1px solid gray;"
    "}"

    "QProgressBar::chunk {"
        "background-color: cadetblue;"
    "}";
}

QString WidgetCSS::checkBox()
{
#ifdef Q_OS_MAC
    return "";
#endif

#ifdef Q_OS_WIN
    return
    "QCheckBox {"
        "spacing: 5px;"
    "}"

    "QCheckBox::disabled {"
        "color:" + disabledColor.name() + ";"
    "}"

    "QCheckBox::indicator {"
        "width: 15px;"
        "height: 15px;"
    "}"

    "QCheckBox::indicator:unchecked {"
        "image: url(:/images/checkbox_unchecked_blue.png);"
    "}"

    "QCheckBox::indicator:checked {"
        "image: url(:/images/checkbox_checked_blue.png);"
    "}"

    "QCheckBox::indicator:disabled {"
    "image: url(:/images/checkbox_disabled.png);"
    "}"

     ;
#endif
}
