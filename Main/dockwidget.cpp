#include "dockwidget.h"
#include "Main/mainwindow.h"

/*
    Try to create your own font that contains the graphic you want at the corresponding
    Unicode code point, such as U+2706 (✆). You can use a font editor such as FontForge or
    Glyphr Studio to design your graphic and assign it to the Unicode character. Then you can
    use the font in your web pages or applications.

    Embed a custom font file in your Qt application using a resource file (*.qrc). You can
    then use the QFontDatabase class to load the font from the resource file and use it in
    your application2. For example:

    // create a resource file with the font file (e.g. BYekan.ttf) under /fonts folder
    <qresource>
        <file alias="BYekan.ttf">fonts/BYekan.ttf</file>
    </qresource>

    // load the font from the resource file using QFontDatabase
    int id = QFontDatabase::addApplicationFont(":/fonts/BYekan.ttf");
    QString family = QFontDatabase::applicationFontFamilies(id).at(0);

    // create a QFont object with the loaded font family
    QFont customFont(family);

    // create a QLabel and set its font and text
    QLabel *label = new QLabel(this);
    label->setFont(customFont);
    label->setText("This is a test");

    Use FontForge directly to create a new font and import your custom characters into it.
    You can then design the shape of each character using the drawing tools and assign it to
    the desired Unicode code point.

    Unicode to QString:  QString s = QString::fromUcs4(0x4FF0);
*/

/* RichTextTabBar *****************************************************************************

RichTextTabBar::RichTextTabBar(QWidget *parent) : QTabBar(parent)
{

}

void RichTextTabBar::setTabText(int index, const QString& text)
{
//    qDebug() << "RichTextTabBar::setTabText" << text << tabRect(index);
    QLabel* label = new QLabel(text);
// cause crash
//    label->setFixedSize(mTabWidth, mTabHeight);
//    label->setStyleSheet("padding-top:0px; padding-bottom:0px; padding-left:5px; padding-right:5px;");
//    setTabButton(index, QTabBar::LeftSide, label);
}

/*
// RichTextTabWidget **************************************************************************
//    Not being used.  Would require also subclassing QMainWindow to use this.


RichTextTabWidget::RichTextTabWidget(QWidget* parent): QTabWidget(parent)
{
    setTabBar(new RichTextTabBar());
}

void RichTextTabWidget::setRichTextTabBar(RichTextTabBar *rtb)
{
    setTabBar(rtb);
}
void RichTextTabWidget::setTabText(int index, const QString &label)
{
    tabBar()->setTabText(index, label);
}
//*/


/* DockTitleBtn *******************************************************************************
A tool button that is placed in the DockTitleBar.  The styling is inherited from DockTitleBar,
which has a border, and must be over-ridden.
*/

BarBtn::BarBtn(/*QWidget *parent*/) : QToolButton()
{
    setStyleSheet(G::css);
    int bg = G::backgroundShade + 30;
    btnHover = QColor(bg,bg,bg);
}

QSize BarBtn::sizeHint() const
{
    return QSize(16,16);
}

// subclass to set opacity of icon on toolbutton
void BarBtn::setIcon(QString path, double opacity)
{
    QPixmap pm(path);
    QImage image(pm.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setOpacity(opacity);
    p.drawPixmap(0, 0, pm);
    p.end();
    QIcon icon(QPixmap::fromImage(image));
    QToolButton::setIcon(icon);
}

// use QToolButton
void BarBtn::setIcon(const QIcon &icon)
{
    QToolButton::setIcon(icon);
}

void BarBtn::setActive(bool on)
{
    /* Override the inherited (border:none) style with a blue accent border when active,
       matching a conventional checked button. Appended after G::css so it wins; cleared
       back to G::css when inactive. */
    if (on)
        setStyleSheet(G::css + "QToolButton { border: 1px solid " + G::appleBlue.name() +
                      "; border-radius: 2px; }");
    else
        setStyleSheet(G::css);
}

//void BarBtn::enterEvent(QEvent*)
//{
//    setStyleSheet
//    (
//        "background:" + btnHover.name() + ";"
//        "border:none;"
//    );
//}

//void BarBtn::leaveEvent(QEvent*)
//{
//    setStyleSheet
//    (
//        "background:transparent;"
//        "border:none;"
//    );
//}

/* DockTitleBar *******************************************************************************
    This replaces the QDockWidget titlebar, enabling the placement of tool buttons.
*/

QSize DockTitleBar::sizeHint() const
{
#ifdef Q_OS_MAC
    int h = style()->pixelMetric(QStyle::PM_TitleBarHeight) * 1.1;
#endif
#ifdef Q_OS_WIN
    int h = style()->pixelMetric(QStyle::PM_TitleBarHeight) * 1.6;
#endif
    return QSize(width(), h);
}

DockTitleBar::DockTitleBar(const QString &title, QHBoxLayout *titleBarLayout) : QWidget()
{
    setStyle();
    setLayout(titleBarLayout);
    titleLabel = new QLabel(this);
    titleLabel->setTextFormat(Qt::RichText);
    setTitle(title);
    titleLabel->setText(title);
    //titleLabel->setPixmap(QPixmap(":/images/icon16/anchor.png"));
    titleBarLayout->addWidget(titleLabel);
    titleBarLayout->addStretch();

}

void DockTitleBar::setTitle(QString title)
{
    titleLabel->setText(title);
    //titleLabel->setPixmap(QPixmap(":/images/icon16/anchor.png"));
}

void showDockToolTip(const QPoint &globalPos, const QString &tip, QWidget *w)
{
    if (tip.isEmpty()) return;
    QPoint pos = globalPos;
#ifdef Q_OS_MAC
    /* macOS places tooltips noticeably further below the cursor than Windows.
       Lift the anchor so the tip sits just under the cursor to match Windows.
       Adjust this value if the gap still looks wrong on the Mac. */
    pos.ry() -= 24;
#endif
    QToolTip::showText(pos, tip, w);
}

void DockTitleBar::setStyle()
{
    // Border color mirrors WidgetCSS::frame() (fm = backgroundShade + 35) so
    // a future change to the frame palette flows through here automatically.
    int fm = G::backgroundShade + 35;
    QString borderColor = QColor(fm, fm, fm).name();
    QString s = "DockTitleBar {"
                "  border: none;"
                "  border-bottom: 1px solid " + borderColor + ";"
                "  font-size:" + G::strFontSize + "pt;"
                "}";
    setStyleSheet(s);
}

void DockTitleBar::paintEvent(QPaintEvent *)
{
/*
    QWidget subclasses do not render stylesheet borders/backgrounds unless
    paintEvent draws the PE_Widget primitive. Without this override the
    border-bottom set in setStyle() is silently dropped.
*/
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void DockTitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
/*
    If this event is not propogated then QDockWidget::toggleTopLevel is not invoked.
*/
    // qDebug() << "DockTitleBar::mouseDoubleClickEvent";
    // event->ignore();
    QWidget::mouseDoubleClickEvent(event);
}

void DockTitleBar::mousePressEvent(QMouseEvent *event)
{
/*
    In solo mode, left-clicking a collapsed dock's title bar expands it.
    The existing collapsedChanged → enforceDockSoloMode flow then collapses
    its siblings in the same area. Outside solo mode (or when the dock is
    not collapsed) the click falls through to default QWidget handling so
    Qt's title-bar drag-to-move behaviour is preserved.
*/
    if (G::useDWCollapse && event->button() == Qt::LeftButton) {
        DockWidget *dock = qobject_cast<DockWidget*>(parentWidget());
        if (dock && dock->isCollapsed() && !dock->isFloating()) {
            MW *mw = qobject_cast<MW*>(dock->parentWidget());
            if (mw && mw->dockSoloModeForArea(mw->dockWidgetArea(dock))) {
                dock->setCollapsed(false);
                event->accept();
                return;
            }
        }
    }
    QWidget::mousePressEvent(event);
}

void DockTitleBar::contextMenuEvent(QContextMenuEvent *event)
{
/*
    Right-click on a custom dock title bar: take the standard QMainWindow
    popup (the list of dock toggles) and append our collapse/expand/solo
    actions scoped to the dock area this title bar belongs to. Floating
    docks fall through to the unmodified base menu.
*/
    DockWidget *dock = qobject_cast<DockWidget*>(parentWidget());
    if (!dock) { QWidget::contextMenuEvent(event); return; }
    MW *mw = qobject_cast<MW*>(dock->parentWidget());
    if (!mw) { QWidget::contextMenuEvent(event); return; }

    QMenu *menu = mw->createPopupMenu();
    if (!menu) menu = new QMenu(this);

    if (G::useDWCollapse && !dock->isFloating()) {
        Qt::DockWidgetArea area = mw->dockWidgetArea(dock);
        menu->addSeparator();
        menu->addAction("Collapse all", mw, [mw, area](){ mw->collapseDocksInArea(area); });
        menu->addAction("Expand all",   mw, [mw, area](){ mw->expandDocksInArea(area); });
        QAction *solo = menu->addAction("Solo mode");
        solo->setCheckable(true);
        solo->setChecked(mw->dockSoloModeForArea(area));
        QObject::connect(solo, &QAction::toggled, mw,
            [mw, area](bool on){ mw->setDockSoloModeForArea(area, on); });
    }

    menu->exec(event->globalPos());
    menu->deleteLater();
}


/* DockWidget *********************************************************************************

    QDockWidget has a feature where you can double click on the title bar and the dock
    will toggle to a floating window and back to its docked state. The problem is if you
    move and resize the floating window and then toggle back to the dock and then back to
    floating again, your position and size are lost.

    This subclass of QDockWidget overrides the MouseButtonDblClick, and
    NonClientAreaMouseButtonDblClick , ignoring Qt's attempts to impose its size and
    location "suggestions". The DockWidget geometry is saved in QSettings for persistence
    between sessions.

    The QDockWidget topLevelChanged signal is not employed because it fires after the
    DockWidget floating status has changed, and we need to know before, so we can save
    the floating geometry.

    When a mouse double click occurs in the docked state, the stored geometry is used to
    re-establish the prior state, factoring in any changes to the screen scale.

    If the DockWidget does not have a custom TitleBarWidget (ie thumbDock), then the
    MouseButtonDblClick event is not triggered when floating, but the
    NonClientAreaMouseButtonDblClick is triggered.

    Before DblClick floating a DockWidget the restore() function is called.from
    MW::setThumbDockFloatFeatures. Before DblClick docking a DockWidget the save()
    function is triggered.

    I have not figured out how to draw a border around the DockWidget, so the contained
    treeview stylesheet is used instead, along with a border around the DockTitleBar.

*/

DockWidget::DockWidget(const QString &title, QString objName, QWidget *parent)
    : QDockWidget(title, parent)
{
    setObjectName(objName);
    isRestoring = false;
}

bool DockWidget::hasCustomTitleBar()
{
    QWidget *titleBarWidget = this->titleBarWidget();
    if (titleBarWidget) return true;
    else return false;
}

void DockWidget::setCollapsed(bool collapse)
{
/*
    Collapse squeezes the dock vertically to just its title bar. The body
    widget is kept VISIBLE (not hidden) so that its sizeHint().width() stays
    in QDockAreaLayout's width calculation — this is what anchors the dock
    area at its pre-collapse width when one or more docks in the area are
    collapsed. The body is forced to zero height by dropping its
    minimumHeight to 0 and clamping the dock's maxHeight to the title bar.

    No explicit width constraint is applied to the dock itself, so the user
    can still drag the dock-area splitter inward and outward while collapsed.
*/
    if (!G::useDWCollapse) return;
    if (collapse == m_isCollapsed) return;
    QWidget *body = widget();
    QWidget *tb = titleBarWidget();
    int titleH = tb ? tb->sizeHint().height() : 22;
    QMainWindow *mw = qobject_cast<QMainWindow*>(parentWidget());

    if (collapse) {
        m_uncollapsedSize = size();
        m_uncollapsedMinH = minimumHeight();
        m_uncollapsedMaxH = maximumHeight();
        if (body) {
            m_uncollapsedBodyMinH = body->minimumHeight();
            body->setMinimumHeight(0);
        }
        /* min = 0 (not titleH) so an adjacent expanded sibling can grow by
        squeezing this collapsed dock smaller. max = titleH keeps the body
        from un-collapsing. The dock still naturally sits at titleH because
        body's sizeHint contribution is 0. */
        setMinimumHeight(0);
        setMaximumHeight(titleH);
        m_isCollapsed = true;
        if (isFloating()) resize(m_uncollapsedSize.width(), titleH);
    } else {
        m_isCollapsed = false;
        if (body) body->setMinimumHeight(m_uncollapsedBodyMinH);

        if (isFloating()) {
            setMinimumHeight(m_uncollapsedMinH);
            setMaximumHeight(m_uncollapsedMaxH);
            if (m_uncollapsedSize.isValid())
                resize(m_uncollapsedSize);
        } else {
            /* Synchronously PIN this dock to its pre-collapse height. The
            pin forces Qt's QDockAreaLayout to allocate targetH to this
            dock (growing the dock area at the central widget's expense if
            necessary) — which resizeDocks alone won't reliably do once the
            area has been compressed by earlier collapses. The pin is
            released on the next event-loop tick so the user can drag the
            inter-dock splitter to resize. */
            int targetH = m_uncollapsedSize.height();
            if (targetH <= titleH) targetH = titleH + 1;
            setMinimumHeight(targetH);
            setMaximumHeight(targetH);

            int restoreMin = m_uncollapsedMinH;
            int restoreMax = m_uncollapsedMaxH;
            QPointer<DockWidget> self(this);
            QTimer::singleShot(0, this, [self, restoreMin, restoreMax]() {
                if (!self) return;
                self->setMinimumHeight(restoreMin);
                self->setMaximumHeight(restoreMax);
            });
        }
    }
    m_isCollapsed = collapse;
    emit collapsedChanged(m_isCollapsed);
}

void DockWidget::toggleCollapsed()
{
    setCollapsed(!m_isCollapsed);
}

bool DockWidget::event(QEvent *event)
{
/*
    QDockWidget title bar overrides MouseButtonDblClick and does not propogate the event
    so it cannot be captured in DockWidget.  Instead capture here.

    The QDockWidget topLevelChanged signal is not employed because it fires after the
    DockWidget floating status has changed, and we need to know before, so we can save
    the floating geometry.
*/
    if (event->type() == QEvent::NonClientAreaMouseButtonDblClick) {

        /* This event is fired when the default (non-custom) floating DockWidget titlebar
        is dblclick. If the DockWidget has a custom titlebar then this event is not
        fired. So, only the thumbDock DockWidget when floating. */

        if (G::isLogger) G::log("DockWidget::event", "QEvent::NonClientAreaMouseButtonDblClick " + objectName());
        qDebug() << "DockWidget::event NonClientAreaMouseButtonDblClick" << objectName()
                 << "isFloating =" << isFloating();
        save();
        setFloating(false);
        emit focus(this);
        // do not propogate event
        return true;
    }

    if (event->type() == QEvent::MouseButtonDblClick) {
        /*  This event is fired for DockWidgets with a custom titlebar when they are both
        floating and docked and non-custom titlebar DockWidgets when docked. */
        if (G::isLogger) G::log("DockWidget::event", "QEvent::MouseButtonDblClick " + objectName());
        /*
        qDebug() << "DockWidget::event MouseButtonDblClick" << objectName()
                 << "isFloating =" << isFloating(); //*/
        if (isFloating() && hasCustomTitleBar()) {
            save();
            setFloating(false);
            emit focus(this);
        }
        else {
            doubleClickDocked = true;
            setDefaultFloatingGeometry();
            setFloating(true);  // must preceed calling restore()
            restore();
        }
        // do not propogate event
        return true;
    }

    return QDockWidget::event(event);
}

void DockWidget::closeEvent(QCloseEvent *event)
{
    qDebug() << "DockWidget::closeEvent";
    emit closeFloatingDock();
    QDockWidget::closeEvent(event);
}

QRect DockWidget::setDefaultFloatingGeometry()
{
    if (G::isLogger) G::log("DockWidget::setDefaultFloatingGeometry");
    // qDebug() << "DockWidget::setDefaultFloatingGeometry parentWidget()->geometry() ="
    //          << parentWidget()->geometry();
    QPoint screenOffset = parentWidget()->geometry().topLeft() +
                          geometry().topLeft() +
                          QPoint(20,20);
    defaultFloatingGeometry = QRect(screenOffset, size());
    return defaultFloatingGeometry;
}

void DockWidget::save()
{
    if (G::isLogger) G::log("DockWidget::save", objectName());
    qDebug() << "DockWidget::save" << objectName() << geometry();
    G::settings->beginGroup((objectName()));
    G::settings->setValue("geometry", frameGeometry());
    // G::settings->setValue("geometry", geometry());
    G::settings->endGroup();
}

void DockWidget::restore()
{
    if (G::isLogger) G::log("DockWidget::restore", objectName());
    doubleClickDocked = false;
    isRestoring = true;
    G::settings->beginGroup((objectName()));
    QRect savedGeometry;
    if (G::settings->contains("geometry")) {
        floatingGeometry = G::settings->value("geometry").toRect();
        savedGeometry = G::settings->value("geometry").toRect();
    }
    else {
        floatingGeometry = defaultFloatingGeometry;
    }
    G::settings->endGroup();
    /*
    qDebug() << "DockWidget::restore" << objectName()
             << "savedGeometry =" << savedGeometry
             << "defaultFloatingGeometry =" << defaultFloatingGeometry
             << "floatingGeometry =" << floatingGeometry
        ; //*/
    // Necessary to pause for the QDockWidget default geometry to be set before overriding.
    G::wait(10);
    setGeometry(floatingGeometry);
}

QRect DockWidget::deconstructSavedGeometry(QByteArray geometry)
{
/*
    From Qwidget::restoreGeometry(const QByteArray &geometry)
*/
    // if (geometry.size() < 4)
    //     return false;
    QDataStream stream(geometry);
    stream.setVersion(QDataStream::Qt_4_0);

    const quint32 magicNumber = 0x1D9D0CB;
    quint32 storedMagicNumber;
    stream >> storedMagicNumber;
    // if (storedMagicNumber != magicNumber)
    //     return false;

    const quint16 currentMajorVersion = 3;
    quint16 majorVersion = 0;
    quint16 minorVersion = 0;

    stream >> majorVersion >> minorVersion;

    // if (majorVersion > currentMajorVersion)
    //     return false;
    // (Allow all minor versions.)

    QRect restoredFrameGeometry;
    QRect restoredGeometry;
    QRect restoredNormalGeometry;
    qint32 restoredScreenNumber;
    quint8 maximized;
    quint8 fullScreen;
    qint32 restoredScreenWidth = 0;

    stream >> restoredFrameGeometry // Only used for sanity checks in version 0
        >> restoredNormalGeometry
        >> restoredScreenNumber
        >> maximized
        >> fullScreen;

    qDebug() << "DockWidget::deconstructSavedGeometry"
             << "\nrestoredFrameGeometry =" << restoredFrameGeometry
             << "\nrrestoredGeometry     =" << restoredGeometry
             << "\nrestoredNormalGeometry =" << restoredNormalGeometry
        ;
    return restoredGeometry;
}

void DockWidget::rpt(QString s)
{
    qDebug() << s
             << "objectName =" << objectName()
             << "isFloating =" << isFloating()
             << "screen =" << QGuiApplication::screens().indexOf(screen())
             << "screen()->geometry() =" << screen()->geometry()
             << "parentWidget()->geometry() =" << parentWidget()->geometry()
             // << "isVisible =" << isVisible()
             << "size =" << size()
             << "geometry() =" << geometry()
             // << "frameGeometry() =" << frameGeometry()
        ;
}

// MAINWINDOW DOCK RELATED FUNCTIONS

QTabBar* MW::tabifiedBar()
{
    // find the tabbar containing the dock widgets
    QTabBar* tabBar = nullptr;
    QList<QTabBar *> tabList = findChildren<QTabBar *>();
    for (int i = 0; i < tabList.count(); i++) {
        if (tabList.at(i)->currentIndex() != -1) {
            tabBar = tabList.at(i);
            break;
        }
    }
    return tabBar;
}

void MW::tabBarAssignRichText(QTabBar *tabBar)
/*
    Not being used. RichTextTabBar not used. Would require also subclassing QMainWindow
    to use this (see dockwidget.cpp line 56).
*/
{
    for (int i = 0; i < tabBar->count(); i++) {
        bool match = tabBar->tabText(i) == folderDockTabText;
        qDebug() << "MW::tabBarAssignRichText" << "tab count =" << tabBar->count()
                 << i << "tabBar->tabText() =" << tabBar->tabText(i)
                 << "folderDockTabText =" << folderDockTabText
                 << "match =" << match;
        if (tabBar->tabText(i) == folderDockTabText) {
            qDebug() << "MW::tabBarAssignRichText match found";
            tabBar->setTabText(i, "xxx");
            //            RichTextTabBar *richTextTabBar = qobject_cast<RichTextTabBar*>(tabBar);
            //            richTextTabBar->setTabText(i, folderDockTabRichText);
        }
    }
}

bool MW::tabBarContainsDocks(QTabBar *tabBar)
{
    if (tabBar == nullptr) return false;
    for (int i = 0; i < tabBar->count(); i++) {
        // qDebug() << "MW::tabBarContainsDocks" << "tab count =" << tabBar->count()
        // << i << "tabBar->tabText() =" << tabBar->tabText(i);
        if (dockTextNames.contains(tabBar->tabText(i))) {
            return true;
        }
    }
    return false;
}

bool MW::isDockTabified(QDockWidget *dock)
{
    // Identify the dock by the object itself (objectName), not by the tab's
    // visible text, so a dock showing a graphic instead of text is still
    // recognised. A dock is tabified if it shares a tab group with others.
    return dock && !tabifiedDockWidgets(dock).isEmpty();
}

QString MW::dockTabToolTip(const QString &tabText)
{
    auto tip = [](const QString &title, const QString &shortcut) {
        // Title in the dock-title blue (mirrors WidgetCSS "DockTitleBar > QLabel").
        // <nobr> stops Qt from word-wrapping the rich-text tooltip.
        return QString(
            "<nobr><span style=\"color:#6CC1E8;\">%1</span>: shortcut %2.</nobr>"
        ).arg(title, shortcut);
    };

    if (tabText == folderDockTabText)   return tip(folderDockTabText,   "F3");
    if (tabText == favDockTabText)      return tip(favDockTabText,      "F4");
    if (tabText == filterDockTabText)   return tip(filterDockTabText,   "F5");
    if (tabText == metadataDockTabText) return tip(metadataDockTabText, "F6");
    if (tabText == thumbDockTabText)    return tip(thumbDockTabText,    "F7");
    if (tabText == embelDockTabText)    return tip(embelDockTabText,    "F8");
    if (tabText == developDockTabText)  return tip(developDockTabText,   "F9");
    return QString();
}

bool MW::isSelectedDockTab(QDockWidget *dock)
{
    // The front (selected) tab in a tabified group is the only one not
    // occluded by its siblings, so its visible region is non-empty. This is
    // determined from the dock object, independent of the tab's label.
    return dock && dock->isVisible() && !dock->visibleRegion().isEmpty();
}

void MW::folderDockVisibilityChange()
{
    if (G::isLogger) G::log("MW::folderDockVisibilityChange");
    if (folderDock->isVisible()) {
        fsTree->scrollToCurrent();
    }
}

void MW::embelDockVisibilityChange()
{
    if (G::isLogger) G::log("MW::embelDockVisibilityChange");

    // loupeDisplay("MW::embelDockVisibilityChange");
    if (turnOffEmbellish) embelProperties->doNotEmbellish();
}

QList<DockWidget*> MW::docksInArea(Qt::DockWidgetArea area) const
{
    QList<DockWidget*> result;
    for (QDockWidget *d : findChildren<QDockWidget*>()) {
        if (d->isFloating()) continue;
        if (dockWidgetArea(d) != area) continue;
        if (DockWidget *dw = qobject_cast<DockWidget*>(d)) result << dw;
    }
    return result;
}

void MW::collapseDocksInArea(Qt::DockWidgetArea area)
{
    for (DockWidget *d : docksInArea(area)) d->setCollapsed(true);
}

void MW::expandDocksInArea(Qt::DockWidgetArea area)
{
    for (DockWidget *d : docksInArea(area)) d->setCollapsed(false);
}

bool MW::dockSoloModeForArea(Qt::DockWidgetArea area) const
{
    return m_dockSoloMode.value(area, false);
}

void MW::setDockSoloModeForArea(Qt::DockWidgetArea area, bool on)
{
    m_dockSoloMode.insert(area, on);
    if (!on) return;
    // Keep the first currently-expanded dock; collapse the rest in this area.
    DockWidget *keep = nullptr;
    for (DockWidget *d : docksInArea(area)) {
        if (!d->isCollapsed()) {
            if (!keep) keep = d;
            else d->setCollapsed(true);
        }
    }
}

void MW::enforceDockSoloMode(DockWidget *justExpanded)
{
    if (!justExpanded || justExpanded->isFloating()) return;
    Qt::DockWidgetArea area = dockWidgetArea(justExpanded);
    if (!m_dockSoloMode.value(area, false)) return;
    for (DockWidget *d : docksInArea(area)) {
        if (d != justExpanded && !d->isCollapsed()) d->setCollapsed(true);
    }
}

void MW::applyDockCollapseState()
{
/*
    Re-apply the persisted collapsed flag for each dock. Must be called AFTER
    restoreState() so each dock has its restored size — that size is what
    setCollapsed(true) snapshots as m_uncollapsedSize for the next expand.
*/
    if (!G::useDWCollapse) return;
    auto apply = [this](DockWidget *d, const QString &key) {
        if (!d) return;
        settings->beginGroup("DockCollapsed");
        bool wasCollapsed = settings->value(key, false).toBool();
        settings->endGroup();
        if (wasCollapsed) d->setCollapsed(true);
    };
    apply(folderDock,   "FolderDock");
    apply(favDock,      "BookmarkDock");
    apply(filterDock,   "FilterDock");
    apply(metadataDock, "MetadataDock");
    apply(thumbDock,    "ThumbDock");
    apply(embelDock,    "EmbelDock");
}

void MW::embelDockActivated(QDockWidget *dockWidget)
{
    if (G::isLogger) G::log("MW::embelDockActivated");
    //    if (dockWidget->objectName() == "embelDock") embelDisplay();
    // enable the folder dock (first one in tab)
    embelDockTabActivated = true;
    QList<QTabBar*> tabList = findChildren<QTabBar*>();
    QTabBar* widgetTabBar = tabList.at(0);
    widgetTabBar->setCurrentIndex(4);
    loupeDisplay("MW::embelDockActivated");
    //    qDebug() << "MW::embelDockActivated" << dockWidget->objectName() << widgetTabBar->currentIndex();

}
