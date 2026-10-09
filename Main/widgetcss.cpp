#include "Main/widgetcss.h"
#include "Main/global.h"

void WidgetCSS::styleGlobals()
{
    const int bg = G::backgroundShade;
    auto shade = [](int s) { return QColor(s, s, s); };

    /* ---------------------------------------------------------------------------------
       TEXT AND HEADER COLOURS
       --------------------------------------------------------------------------------- */
    G::textShade = 190;                         // text default luminousity
    G::textColor = shade(G::textShade);
    G::disabledColor = shade(bg + 40);
    G::header1Color = QColor(108,193,232);      // #6cc1e8  dock titles
    G::header2Color = QColor(81,141,169);       // #518da9
    G::header3Color = QColor(66,115,138);       // #42738a
    // G::header3Color = QColor(50,82,98);
    /* Caption colour for the subpanel headers inside a Develop scope (Basic, Color, Mask,
       Submasks, ...). Not the Scope or Raw bands, which keep header2Color. */
    G::scopeSubheaderColor = G::textColor;
    G::helpColor = QColor(37,65,40);
    G::appleBlue = QColor(21,113,211);          // #1571D3
    G::unfiledKeywordColor = QColor(0x9a, 0x5a, 0x5a);  // Filters + Keywords dock
    G::appliedKeywordColor = QColor(0xb4, 0xa4, 0x5a);  // Keywords dock: on this image

    /* ---------------------------------------------------------------------------------
       WIDGET CHROME
       --------------------------------------------------------------------------------- */
    G::borderColor = shade(bg + 40);
    G::tabWidgetBorderColor = shade(bg + 60);
    G::pushButtonBackgroundColor = shade(bg - 10);
    G::frameLineColor = shade(bg + 15);         // menu background shade (mb)
    G::frameLineWidth = 1;
    G::frameLineRadius = 6;                     // = brInteractive (buttons)
    G::scrollBarHandleBackgroundColor = QColor(bg, bg + 3, bg);
    G::selectionColor = QColor(68,95,118);      // #445f76
    G::mouseOverColor = QColor(40,54,66);
    G::iconOpacity = 0.5;                       // 0.0 - 1.0 (higher is brighter)

    /* Pick label colours */
    const int transparency = 50;
    G::labelNoneColor = QColor(85,85,85,transparency);  // Background Gray
    G::labelRedColor = QColor(60,20,20);                // Dark red
    G::labelYellowColor = QColor(80,55,15);             // Dark yellow
    G::labelGreenColor = QColor(20,40,20);              // Dark green
    G::labelBlueColor = QColor(20,20,60);               // Dark blue
    // G::labelBlueColor = QColor(20,45,100);
    // G::labelBlueColor = QColor(32,58,124);
    G::labelPurpleColor = QColor(50,30,70);             // Dark purple
    // G::labelPurpleColor = QColor(60,30,90);
    // G::labelPurpleColor = QColor(54,37,95);

    /* Named palette */
    G::lightgray = "#aaaaaa";
    G::darkgray = "#111111";
    G::lightpurple = "#5f496e";
    G::darkpurple = "#3d0066";
    G::lightblue = "#1f4e85";
    G::darkblue = "#0a1633";
    G::lightyellow = "#857a1f";
    G::darkyellow = "#1a1800";
    G::lightorange = "#854a1f";
    G::darkorange = "#1a0d00";
    G::lightred = "#850000";
    G::darkred = "#110000";
    G::lightcyan = "#1f8585";
    G::darkcyan = "#001a1a";
    G::lightgreen = "#1f851f";
    G::darkgreen = "#001a00";
    G::lightteal = "#1f8567";
    G::darkteal = "#00261c";
    G::lightmaroon = "#5e1a1a";
    G::darkmaroon = "#1a0000";
    G::lightpink = "#854963";
    G::darkpink = "#3d0022";
    G::lightmagenta = "#850085";
    G::darkmagenta = "#1a001a";

    /* SECTION HEADER GRADIENT: the vertical top -> bottom band behind every section
       header -- property-tree root rows (PropertyDelegate: Develop sections, Preferences,
       Embellish), the Filters categories, the InfoView categories, and the widget-built
       bands (GradientHeader, ScopeHeader, RawPanel, MaskPanel, Transform, Replace,
       History). */
    G::headerGradientTop = shade(bg + 5);
    G::headerGradientBottom = shade(qMax(0, bg - 15));

    /* ---------------------------------------------------------------------------------
       DEVELOP PANEL SURFACES AND SEPARATORS
       --------------------------------------------------------------------------------- */
    /* SUBPANEL CONTENT background: ten shades above the dock's own background, so each
       subpanel's contents (the scopes strip, the Raw and Transform bodies, the Edits
       tree's non-header rows) read as a surface distinct from the header bands over them
       -- those keep the headerGradient band -- and from the action row under the dock
       title bar, which stays on the plain background. */
    G::panelContentBg = shade(bg + 10);

    /* CONTROL-GROUP SEPARATOR: the rule drawn BETWEEN groups of controls inside a
       subpanel -- the dividers between the Basic tone/WB/presence groups in the Edits
       tree (PropertyEditor::addDivider), the Raw panel's rule under "Render using" and
       the Mask panel's rule above Submasks. Brighter than the lifted content it sits on
       (panelContentBg + 20), so a group break reads inside a panel without competing
       with the panel separator below.
         Color   line colour
         Width   line thickness, px
         Height  vertical space the separator occupies (the line is centred in it)
         Inset   clear space from each side of the panel to the line's ends */
    G::groupSeparatorColor = shade(bg + 30);
    G::groupSeparatorWidth = 1;
    G::groupSeparatorHeight = 8;
    G::groupSeparatorInset = 6;

    /* PANEL SEPARATOR: the rule along the bottom edge of each Develop subpanel, and under
       the dock's action row. The same shade the dock frames use (l5, drawn by the
       QMainWindow::separator rule in mainWindow()) so a panel boundary inside a dock
       reads as the same kind of structural division as the boundary between docks --
       quieter than the group rules inside a panel. Its height is panelBorderHeight
       below. */
    G::panelSeparatorColor = shade(bg + 5);

    /* ---------------------------------------------------------------------------------
       LAYOUT METRICS
       --------------------------------------------------------------------------------- */
    G::scrollBarThickness = 14;        // Also set in winnowstyle.css for vertical and horizontal
    G::propertyWidgetMarginLeft = 5;
    G::propertyWidgetMarginRight = 2;
    /* Clear space between an expand/collapse arrow and the header title it precedes, in the
       Develop/property panels: the property tree rows (PropertyDelegate) and the widget
       header bands (RawPanel, MaskPanel, SubmaskList). Raise it to give crowded titles more
       breathing room. */
    G::decorationTitleGap = 3;
    /* Develop Scope/Edits panel TRAILING BUTTONS: every header and row there ends with the
       same [eye] [:] pair (see ScopeHeader / SubmaskList / DevelopProperties::addHeader).
       headerBtnGap is the clear space between the two; headerBtnRightInset is the space from
       the panel's right edge to the menu button. Both are shared by the widget headers and by
       the tree's section headers (BarBtnEditor), which is what makes the pair line up on
       every line. */
    G::headerBtnGap = 6;
    G::headerBtnRightInset = 6;
    /* Develop SUB-HEADER OFFSET: the Edits header (ScopeHeader) collapses everything below
       it, so the headers it hides -- the Mask and Submasks bands and the tree's Basic /
       Color / ... sections -- are offset this far right of it, arrow AND title, to read as
       its children. Only the header content shifts: the rows under each sub-header keep
       their own indentation, so the sliders keep their full width. 0 lines them all up flush
       again. */
    G::subHeaderIndent = 10;
    /* Develop HEADER LEFT INSET: clear space from the panel's left edge to the expand/
       collapse arrow of EVERY header in the dock -- the Raw band, the Edits (scope) band and
       the sub-headers it folds away (Mask, Submasks and the tree's Basic / Color / ...
       sections). The arrows otherwise start hard against the edge. Applied on top of
       subHeaderIndent for the sub-headers, so they keep their offset under the Edits arrow,
       and only to the header content: the rows under each header are untouched. 0 puts the
       arrows back against the edge. */
    G::headerLeftInset = 5;
    /* Develop PANEL SEPARATOR: every Develop dock panel (Raw, Edits, Mask,
       Transform, Fill Replace) draws a rule this high across its bottom edge in
       G::panelSeparatorColor, so stacked panels read as distinct blocks. Each panel
       reserves the space as its layout's bottom margin. 0 removes the rule everywhere. */
    G::panelBorderHeight = 3;
    /* Develop HEADER CAPTION TRIM: PropertyDelegate::paint centres a section header's
       arrow and caption on the row LESS this many pixels at the bottom (its r4), which
       sits them fractionally high in the band -- the tree's own look. The widget-built
       header bands (RawPanel, MaskPanel, SubmaskList) reproduce it as their layout's
       bottom margin, with no top margin, so "Raw" and "Mask" sit at the same height in
       their bands as "Basic" and "Color" do in theirs. 0 centres everything instead. */
    G::headerCaptionTrim = 3;
}

QString WidgetCSS::css()
{
    styleGlobals();
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

    textColor = G::textColor;
    disabledColor = G::disabledColor;
    header1Color = G::header1Color;
    header2Color = G::header2Color;
    header3Color = G::header3Color;
    borderColor = G::borderColor;
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
    brInteractive  = QString::number(G::frameLineRadius) + "px";   // panel corners too
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
            panelFrame() +
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
    /* The separators between docks and the central widget are invisible (window
       background) until the mouse is over one; the frameLines already mark the edges. */
    return
    "QMainWindow::separator {"
        "background: " + QColor(bg,bg,bg).name() + ";"
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
        // "color: #6CC1E8;"  // this works
        "color: " + header1Color.name() + ";" // header1Color.name() does not work
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
        "border: " + frameLine() + ";"
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
        "border: " + frameLine() + ";"
        "padding-top: 2px;"
        "padding-bottom: 3px;"
        "padding-left: 5px;"
        "padding-right: 5px;"
    "}"

    "QTabBar::tab:selected {"
        "color:" + G::textColor.name() + ";"
        "border-bottom: 0px;"
        "background-color: " + G::selectionColor.name() + ";"
    " }"

    "QTabBar::tab:disabled {"
        "color:" + disabledColor.name() + ";"
    "}"
    ;
}

QString WidgetCSS::frameLine()
{
    return QString::number(G::frameLineWidth) + "px solid " + G::frameLineColor.name();
}

QString WidgetCSS::panelFrame()
{
/*
    Every panel's content sits in a FrameLineBox ("DockFrame", see DockWidget::setWidget)
    that draws the frameLine round it. A tree or stacked widget that IS the content fills
    the box edge to edge, so its own border would sit right against the frameLine and
    double it: those borders go. (They were the stand-in for a panel border -- see the
    DockWidget notes.) A tree nested deeper, inset by its container's margins, keeps its
    border as an inner box.
*/
    return
    "QWidget#DockFrame > QTreeView,"
    "QWidget#DockFrame > QTreeWidget,"
    "QWidget#DockFrame > QStackedWidget,"
    "QWidget#DockFrame > QStackedWidget > QTreeView,"
    "QWidget#DockFrame > QStackedWidget > QTreeWidget {"
        "border: none;"
    "}";
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

QString WidgetCSS::itemViewIndicator(const QString &view)
{
/*
    CHECKABLE ITEMS IN AN ITEM VIEW DRAW THEIR OWN INDICATOR, ON BOTH PLATFORMS.

    Windows needed this because the native indicator did not match the G::css QCheckBox
    style.  macOS needs it because as of macOS 27 the macOS style draws NOTHING for an
    item-view check indicator: the boxes vanished from the Filters panel altogether,
    while standalone QCheckBox widgets (and the Preferences tree, which paints its own)
    were untouched.  Reproduced against both Qt 6.9.2 and Qt 6.11.0, so it is the OS and
    not a Qt build -- a Qt update will not bring the boxes back.

    Applied to QTreeView, QListView and QTableView because all three carry checkable
    items: Filters and the Catalog keyword tree, the Save Develop Preset tree, the
    recurse column in Manage Catalog, and the Find Duplicates list.

    THE INDETERMINATE RULE IS NOT DECORATION.  PartiallyChecked is how Filters and the
    Catalog tree show an EXCLUDED item, and how the preset tree shows a part-selected
    group; without a rule for it that state falls back to the same missing native
    indicator.
*/
    return
    view + "::indicator {"
        "width: 15px;"
        "height: 15px;"
    "}"

    + view + "::indicator:unchecked {"
        "image: url(:/images/checkbox_unchecked_blue.png);"
    "}"

    + view + "::indicator:checked {"
        "image: url(:/images/checkbox_checked_blue.png);"
    "}"

    + view + "::indicator:indeterminate {"
        "image: url(:/images/checkbox_indeterminate_blue.png);"
    "}"

    + view + "::indicator:disabled {"
        "image: url(:/images/checkbox_disabled.png);"
    "}"
    ;
}

QString WidgetCSS::listView()
{
    return
    "QListView {"
        "border: 1px solid " + QColor(bg,bg,bg).name() + ";"
    "}"
    + itemViewIndicator("QListView")
    ;
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
        "border: " + frameLine() + ";"
    "}"

    /*  THE SOURCE PANEL'S TWO TREES SIT DIRECTLY UNDER ITS TITLE BAR, which already draws
        a rule, so a top border here would double it. Both trees drop it, so switching the
        Folders | Library toggle does not move a line. Keyed on objectName here rather
        than set on the widget, because MW::setFontSize and setBackgroundShade re-apply
        the whole of G::css to it and would wipe a widget-level rule. */
    "QTreeView#fsTree, QTreeWidget#libTree {"
        "border-top: none;"
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

    + itemViewIndicator("QTreeView")
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
    "}"
    + itemViewIndicator("QTableView")
    ;
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
    "}"
    /* LAST so it beats the two above (same specificity, later rule wins): neither of
       them is qualified with :enabled, so without this a disabled radio kept its
       cadetblue / silver text -- e.g. the Raw panel's "Raw / Embedded Preview" pair on a
       greyed Develop panel. */
    "QRadioButton:disabled {"
        "color:" + disabledColor.name() + ";"
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
/*
    QPlainTextEdit IS NOT A QTextEdit -- it derives from QAbstractScrollArea, so a rule
    written for QTextEdit does not reach it and it falls back to the palette's Base,
    which is the wrong shade beside every other edit box in the app. It is named here
    rather than styled where it is used, so a plain text edit added anywhere matches the
    line edits and text edits around it without knowing about this file.
*/
    return
    "QTextBrowser, QTextEdit, QPlainTextEdit {"
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
