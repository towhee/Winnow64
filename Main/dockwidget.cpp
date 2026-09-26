#include "dockwidget.h"
#include "Main/mainwindow.h"
#include "Utilities/panelprobe.h"
#include <QTimer>
#include <QToolBar>

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
    /* Override the inherited (border:none) style with a white accent border when active,
       matching a conventional checked button. White reads louder than the blue accent
       against the dark panel background, which is the point: the active tool must be
       obvious at a glance. Appended after G::css so it wins; cleared back to G::css
       when inactive. */
    /* The accent border dims with the panel: white on a greyed action row reads as the
       one live control left. G::dimmed keeps it the same border, just deactivated, so
       which tool is armed is still legible. */
    if (on)
        setStyleSheet(G::css + "QToolButton { border: 1px solid white;"
                               " border-radius: 2px; }"
                               "QToolButton:disabled { border: 1px solid " +
                               G::dimmed(Qt::white).name() + "; border-radius: 2px; }");
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

/* FrameLineBox ************************************************************************/

FrameLineBox::FrameLineBox(QWidget *parent) : QWidget(parent)
{
    setSides(m_sides);
}

void FrameLineBox::setSides(Qt::Edges sides)
{
/*
    The content is inset by the frameLine on each drawn side through the widget's
    contents margins, which any layout set on the box respects.
*/
    m_sides = sides;
    const int w = G::frameLineWidth;
    setContentsMargins(sides & Qt::LeftEdge ? w : 0, sides & Qt::TopEdge ? w : 0,
                       sides & Qt::RightEdge ? w : 0, sides & Qt::BottomEdge ? w : 0);
    update();
}

void FrameLineBox::paintEvent(QPaintEvent *)
{
    /* Read at paint time, so a background shade change (WidgetCSS::css) carries over. */
    QPainter p(this);
    const int w = G::frameLineWidth;
    const QRect r = rect();
    const QColor c = G::frameLineColor;
    if (m_sides & Qt::TopEdge)    p.fillRect(0, 0, r.width(), w, c);
    if (m_sides & Qt::BottomEdge) p.fillRect(0, r.height() - w, r.width(), w, c);
    if (m_sides & Qt::LeftEdge)   p.fillRect(0, 0, w, r.height(), c);
    if (m_sides & Qt::RightEdge)  p.fillRect(r.width() - w, 0, w, r.height(), c);
}

/* DockTitleBar ************************************************************************/

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
    /* The panel's frameLine runs up the sides and across the top of the title bar (see
       paintEvent); the panel's FrameLineBox draws the rest. Inset so no child covers it. */
    const int w = G::frameLineWidth;
    setContentsMargins(w, w, w, 0);
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
                "  border: none;";
    if (bottomBorder)
        s += "  border-bottom: 1px solid " + borderColor + ";";
    s +=        "  font-size:" + G::strFontSize + "pt;"
                "}";
    setStyleSheet(s);
}

void DockTitleBar::setBottomBorderVisible(bool visible)
{
    bottomBorder = visible;
    setStyle();
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

    /* The panel's frameLine: left, top and right. The rule under the title (setStyle)
       is the divider, not the border, and stays as it is. */
    const int w = G::frameLineWidth;
    const QColor c = G::frameLineColor;
    p.fillRect(0, 0, width(), w, c);
    p.fillRect(0, 0, w, height(), c);
    p.fillRect(width() - w, 0, w, height(), c);
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

void DockWidget::setWidget(QWidget *content)
{
/*
    EVERY PANEL IS BORDERED BY THE FRAMELINE. QDockWidget draws no border of its own (and
    its layout ignores contents margins), so the content goes in a FrameLineBox, named
    "DockFrame" for the stylesheet (WidgetCSS::panelFrame drops the border of a tree or
    stacked widget that fills it, which would otherwise double the line).
*/
    if (!m_frame) {
        m_frame = new FrameLineBox(this);
        m_frame->setObjectName("DockFrame");
        QVBoxLayout *layout = new QVBoxLayout(m_frame);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        QDockWidget::setWidget(m_frame);
    }
    QLayout *layout = m_frame->layout();
    if (m_content && m_content != content) {
        layout->removeWidget(m_content);
        m_content->setParent(nullptr);
    }
    m_content = content;
    if (content) {
        layout->addWidget(content);
        /* The DockFrame rules match on ancestry, which just changed: re-polish the
           content and the stacked pages below it. */
        QList<QWidget *> ws{content};
        for (QObject *o : content->children())
            if (QWidget *cw = qobject_cast<QWidget *>(o)) ws << cw;
        for (QWidget *w : ws) {
            w->style()->unpolish(w);
            w->style()->polish(w);
        }
    }
    syncFrameSides();
}

void DockWidget::setTitleBarWidget(QWidget *titleBar)
{
    QDockWidget::setTitleBarWidget(titleBar);
    syncFrameSides();
}

void DockWidget::setFrameLineVisible(bool visible)
{
    m_frameLineVisible = visible;
    syncFrameSides();
}

void DockWidget::syncFrameSides()
{
/*
    Under a DockTitleBar the box leaves its top side off: the title bar carries the
    frameLine across the top and down its sides, and its own rule is the divider. With
    Qt's title bar (the thumb dock) the box draws all four sides; with the frameLine off
    (the Module dock, setFrameLineVisible) it draws none.
*/
    if (!m_frame) return;
    if (!m_frameLineVisible) {
        m_frame->setSides({});
        return;
    }
    Qt::Edges sides = Qt::LeftEdge | Qt::RightEdge | Qt::BottomEdge;
    if (!qobject_cast<DockTitleBar *>(titleBarWidget())) sides |= Qt::TopEdge;
    m_frame->setSides(sides);
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
        /* body is the FrameLineBox; the content's own minimum would hold it open. */
        if (m_content) {
            m_uncollapsedContentMinH = m_content->minimumHeight();
            m_content->setMinimumHeight(0);
        }
        /* min = 0 (not titleH) so an adjacent expanded sibling can grow by
        squeezing this collapsed dock smaller. max = titleH keeps the body
        from un-collapsing. The dock still naturally sits at titleH because
        body's sizeHint contribution is 0. */
        setMinimumHeight(0);
        setMaximumHeight(titleH);
        /*  A collapse is a min/max PIN, which is the other way a panel ends up a size
            nobody asked for -- and the pin outlives the call, so it is recorded where it
            is applied rather than inferred later from a geometry.  See
            Utilities/panelprobe.h. */
        if (G::isPanelProbe)
            PanelProbe::Instance().NoteConstraint(objectName(), "collapse pin height",
                                                  0, titleH);
        m_isCollapsed = true;
        if (isFloating()) resize(m_uncollapsedSize.width(), titleH);
    } else {
        m_isCollapsed = false;
        if (body) body->setMinimumHeight(m_uncollapsedBodyMinH);
        if (m_content) m_content->setMinimumHeight(m_uncollapsedContentMinH);

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
            if (G::isPanelProbe)
                PanelProbe::Instance().NoteConstraint(objectName(), "expand pin height",
                                                      targetH, targetH);

            int restoreMin = m_uncollapsedMinH;
            int restoreMax = m_uncollapsedMaxH;
            QPointer<DockWidget> self(this);
            QTimer::singleShot(0, this, [self, restoreMin, restoreMax]() {
                if (!self) return;
                self->setMinimumHeight(restoreMin);
                self->setMaximumHeight(restoreMax);
                /*  The pin is released a tick later, so the height the dock KEEPS is
                    decided after this, not by targetH above. */
                if (G::isPanelProbe)
                    PanelProbe::Instance().NoteConstraint(self->objectName(),
                                                          "release pin height",
                                                          restoreMin, restoreMax);
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

bool DockWidget::isTitleBarPos(const QPoint &globalPos) const
{
/*
    A double-click on anything inside the dock body (a subpanel title bar, a tree row, a
    label ...) that does not consume the event bubbles up to DockWidget::event. Only a
    double-click on the dock's OWN title bar should float/dock the panel, so the click
    position is tested against the title bar before acting on it.
*/
    const QPoint pos = mapFromGlobal(globalPos);
    if (QWidget *tb = titleBarWidget())
        return tb->isVisible() && tb->geometry().contains(pos);
    /* Default (Qt drawn) title bar: the title area is everything above the body. */
    if (QWidget *body = widget())
        return pos.y() < body->geometry().top();
    return true;
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
        floating and docked and non-custom titlebar DockWidgets when docked.  It is also
        fired for double clicks anywhere in the dock body that bubble up from a child
        that ignored them (ie a subpanel title bar), so only act on the dock title bar. */
        auto *e = static_cast<QMouseEvent*>(event);
        if (!isTitleBarPos(e->globalPosition().toPoint()))
            return QDockWidget::event(event);
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
    if (tabText == filterDockTabText) {
        /* One panel, two scopes, ONE search key: F2 searches whichever scope is current
           (Folders = what is loaded, Catalog = everything indexed). F5 still raises the
           panel itself. */
        if (G::useFilterPanel)
            return QString("<nobr><span style=\"color:#6CC1E8;\">Filters</span>: "
                           "shortcut F5. F2 searches the current scope.</nobr>");
        return tip(filterDockTabText, "F5");
    }
    /* Catalog searches the INDEX (every image seen), where Filters searches the
       datamodel (what is loaded). Not reached with G::useFilterPanel -- there is no
       Catalog tab then. */
    if (!catalogDockTabText.isEmpty() && tabText == catalogDockTabText)
        return QString("<nobr><span style=\"color:#6CC1E8;\">%1</span>: "
                       "Window > Catalog Panel.</nobr>").arg(catalogDockTabText);
    /*  Keywords has NO SHORTCUT, deliberately: F3-F9 are taken by the other docks and
        F10-F12 are spoken for, and inventing a modifier chord for a panel that is off by
        default would be a key nobody finds. It is reached from Window > Keywords Panel,
        which is also where the user discovers it exists. */
    if (!keywordsDockTabText.isEmpty() && tabText == keywordsDockTabText)
        return QString("<nobr><span style=\"color:#6CC1E8;\">%1</span>: "
                       "Window > Keywords Panel.</nobr>").arg(keywordsDockTabText);
    if (tabText == metadataDockTabText) return tip(metadataDockTabText, "F6");
    if (tabText == thumbDockTabText)    return tip(thumbDockTabText,    "F7");
    if (tabText == embelDockTabText)    return tip(embelDockTabText,    "F8");
    if (tabText == developDockTabText)  return tip(developDockTabText,   "F9");
    /* History is Develop-only, so its keys are Develop mode local ("H" for the panel,
       "P" for its Presets section, dispatched by the arbiter) rather than F-keys like the
       docks above -- the tab is only ever seen in Develop mode, where H and P work. */
    if (tabText == historyDockTabText)
        return QString("<nobr><b>%1</b> panel &nbsp;(H, or P for presets)</nobr>")
               .arg(historyDockTabText);
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

/* ----------------------------------------------------------------------------------------
   Show/hide bars -- one strip per window rim, each collapsing its whole side
   (see Main/showhidebar.h for the widget and the reasoning behind the triangle)
   ---------------------------------------------------------------------------------------- */

void MW::createShowHideBars()
{
/*
    Build the three bars and the toolbars that put them on the window rim.

    TOOLBARS, NOT DOCKS. QMainWindow lays its toolbar areas out as a ring OUTSIDE the
    dock areas, so a left toolbar is outboard of the left panels -- which is the rim, and
    is the only way to reach it: a dock area can only be APPENDED to (Qt has no prepend),
    so a dock added to the left area lands between the panels and the photo. The first
    attempt at this was a dock and sat in exactly that wrong place.

    Being toolbars also keeps them out of MW::docksInArea(), which filters with
    qobject_cast<DockWidget*>, and so out of the collapse / solo-mode / dock-tab
    machinery every real panel is inside. Nothing has to remember to skip them.
*/
    if (G::isLogger) G::log("MW::createShowHideBars");

    struct Spec { ShowHideBar::Edge edge; Qt::DockWidgetArea area; Qt::ToolBarArea tbArea;
                  const char *objName; ShowHideBar **bar; QToolBar **tb; const char *tip; };
    const Spec specs[] = {
        {ShowHideBar::Left,   Qt::LeftDockWidgetArea,   Qt::LeftToolBarArea,
         "LeftShowHideBar",   &leftBar,   &leftBarDock,   "Hide or show the panels on the left"},
        {ShowHideBar::Right,  Qt::RightDockWidgetArea,  Qt::RightToolBarArea,
         "RightShowHideBar",  &rightBar,  &rightBarDock,  "Hide or show the panels on the right"},
        {ShowHideBar::Bottom, Qt::BottomDockWidgetArea, Qt::BottomToolBarArea,
         "BottomShowHideBar", &bottomBar, &bottomBarDock, "Hide or show the thumbnails"},
        {ShowHideBar::Top,    Qt::TopDockWidgetArea,    Qt::TopToolBarArea,
         "TopShowHideBar",    &topBar,    &topBarDock,    "Hide or show the Module panel"},
    };

    for (const Spec &sp : specs) {
        ShowHideBar *bar = new ShowHideBar(sp.edge, this);
        bar->setToolTip(sp.tip);
        QToolBar *tb = new QToolBar(this);
        tb->setObjectName(sp.objName);
        /* Everything that makes a toolbar look and behave like a toolbar, off: no drag
           handle, no float, no right-click "hide this toolbar" menu (which would leave
           the user with a bar they could not get back), no frame, no padding. What is
           left is a strip holding one widget. */
        tb->setMovable(false);
        tb->setFloatable(false);
        tb->setAllowedAreas(sp.tbArea);
        tb->setContextMenuPolicy(Qt::PreventContextMenu);
        tb->setStyleSheet("QToolBar { border: none; padding: 0; margin: 0;"
                          " spacing: 0; background: transparent; }");
        tb->layout()->setContentsMargins(0, 0, 0, 0);
        tb->addWidget(bar);
        if (sp.edge == ShowHideBar::Bottom || sp.edge == ShowHideBar::Top)
            tb->setFixedHeight(ShowHideBar::thickness());
        else                                tb->setFixedWidth(ShowHideBar::thickness());
        const Qt::DockWidgetArea area = sp.area;
        connect(bar, &ShowHideBar::clicked, this, [this, area]{ toggleDockArea(area); });
        addToolBar(sp.tbArea, tb);
        *sp.bar = bar;
        *sp.tb = tb;
    }

    /* A panel opening or closing by any other route -- F3-F9, the View menu, a workspace
       -- changes whether its side has anything to hide, so the bars re-evaluate. Wired
       to the real panels only; the bars' own docks would recurse. */
    for (QDockWidget *d : findChildren<QDockWidget*>()) {
        if (!qobject_cast<DockWidget*>(d)) continue;      // skips the bars themselves
        connect(d, &QDockWidget::visibilityChanged, this, [this]{ syncShowHideBars(); });
    }
}

void MW::placeShowHideBars()
{
/*
    Put each bar back on its edge of the window.

    Called after every path that lays docks out -- restoreWindowState,
    builtInDefaultWorkspace, invokeWorkspace -- and unconditionally, not only when a
    saved state predates them. The bars cannot be moved, floated or closed by the user,
    so there is never a position of theirs worth preserving; re-asserting is always
    right, and it means the feature does not depend on any saved state being correct.
    That is also why they have no placeDocksAddedSince rows (see winnowStateVersion).

    addToolBar on an area that already holds the bar is a move, not a duplicate, so this
    is safe to re-issue. Toolbar areas ring the dock areas, so each bar lands outboard of
    the panels it acts on however those panels are arranged -- nothing here has to know
    the layout, which is what two earlier dock-based attempts did have to and got wrong
    (see createShowHideBars).
*/
    if (G::isLogger) G::log("MW::placeShowHideBars");
    if (!leftBarDock || !rightBarDock || !bottomBarDock || !topBarDock) return;

    addToolBar(Qt::LeftToolBarArea,   leftBarDock);
    addToolBar(Qt::RightToolBarArea,  rightBarDock);
    addToolBar(Qt::BottomToolBarArea, bottomBarDock);
    addToolBar(Qt::TopToolBarArea,    topBarDock);

    /*  The Module dock is pinned the same way. It cannot be moved or floated, so a
        position of its own is never a user choice -- and a state saved before it existed
        (winnowStateVersion < 8) leaves it wherever Qt drops it. Re-adding a dock that is
        already in the top area is a no-op move. */
    if (moduleDock && (moduleDock->isFloating()
                       || dockWidgetArea(moduleDock) != Qt::TopDockWidgetArea))
        addDockWidget(Qt::TopDockWidgetArea, moduleDock);

    syncShowHideBars();
}

QAction *MW::dockVisibleAction(QDockWidget *dock) const
{
    if (!dock) return nullptr;
    if (dock == folderDock)   return folderDockVisibleAction;
    if (dock == favDock)      return favDockVisibleAction;
    if (dock == filterDock)   return filterDockVisibleAction;
    if (dock == catalogDock)  return catalogDockVisibleAction;
    if (dock == keywordsDock) return keywordsDockVisibleAction;
    if (dock == metadataDock) return metadataDockVisibleAction;
    if (dock == thumbDock)    return thumbDockVisibleAction;
    if (dock == embelDock)    return embelDockVisibleAction;
    if (dock == developDock)  return developDockVisibleAction;
    if (dock == historyDock)  return historyDockVisibleAction;
    if (dock == moduleDock)   return moduleDockVisibleAction;
    return nullptr;
}

void MW::toggleDockArea(Qt::DockWidgetArea area)
{
/*
    A bar was clicked: collapse its side, or restore exactly what was there.

    The ...DockVisibleAction checked states are deliberately NOT touched. They are the
    user's own per-panel preference, set from the View menu and F3-F9, and a bar that
    rewrote them could not tell a panel the user closed on purpose from one it hid
    itself -- reopening would resurrect the lot. Instead the visible set is remembered in
    areaCollapsed and put back verbatim.

    SIZES ARE REMEMBERED TOO, and re-applied with resizeDocks on the way back. Showing a
    dock again does not restore the extent it had: the dock area redistributes what it
    has among whatever is now visible, so a panel the user had dragged to a particular
    width came back at some other width, which reads as the bar having damaged the
    layout rather than merely hidden it.

    TAB GROUPS ARE THE SUBTLE PART, and the first version got it wrong in a way that was
    visible immediately. In a tabified group only the CURRENT tab is un-hidden; the ones
    behind it are isHidden() exactly as a panel the user closed with F3 is. Collapsing on
    "not hidden" therefore hid only the front tab -- Qt promoted one of its siblings to
    the front, the group shrank to what those siblings wanted, and expanding brought the
    original back as a BACK tab at the wrong width. So:

      . what the user WANTS is the visible-action's checked state, not isHidden(). That
        is what tells a back tab (still wanted) from a closed panel (not wanted), and it
        is read here, never written -- see above.
      . which tab was FRONT is recorded separately and raised again on the way back.
*/
    if (G::isLogger) G::log("MW::toggleDockArea");

    /* The map is updated BEFORE the docks are, both ways round. Each setVisible fires
       visibilityChanged, which runs syncShowHideBars -- and a sync that ran while the
       map still disagreed with the docks would judge the area empty and take the bar
       off screen, then put it back a moment later. */
    /* Which way a dock's extent runs in this area: left and right panels are sized by
       width, the filmstrip by height. */
    const bool horizontal = (area == Qt::LeftDockWidgetArea || area == Qt::RightDockWidgetArea);
    const Qt::Orientation orient = horizontal ? Qt::Horizontal : Qt::Vertical;

    if (areaCollapsed.contains(area)) {
        const QList<CollapsedDock> restore = areaCollapsed.take(area);
        const int extent = areaCollapsedExtent.take(area);
        QPointer<QDockWidget> front;
        for (const CollapsedDock &c : restore) {
            if (!c.dock) continue;
            c.dock->setVisible(true);
            if (c.wasCurrentTab && !front) front = c.dock;   // FIRST, not last
        }
        /* Both the size and the tab go back DEFERRED, after the layout has taken the
           re-shown docks into account. Issued inline they do not survive: resizeDocks is
           overwritten by the settling pass, and raise() is overwritten by whatever Qt
           and MW's own tab bookkeeping (scheduleDockTabUpdate, restoreDockTabSelection)
           decide while the group is being rebuilt -- showing the members in list order
           otherwise leaves the LAST one raised, which is how re-opening the Source group
           came back on Bookmarks. One representative dock is enough for resizeDocks and
           is what a tab group wants: it applies a tabbed group's size through any
           member. */
        if (front)
            QTimer::singleShot(0, this, [this, front, extent, orient]{
                if (!front || front->isHidden()) return;
                if (extent > 0) resizeDocks({front}, {extent}, orient);
                front->raise();          // last: the tab the user was looking at
            });
    }
    else {
        QList<CollapsedDock> wanted;
        int extent = 0;
        for (DockWidget *d : docksInArea(area)) {
            /* The ACTION, not isHidden(): a back tab is hidden too, and it has to come
               back. A panel the user closed has its action unchecked and is left alone.
               Falling back to !isHidden() covers a dock with no action of its own. */
            const QAction *a = dockVisibleAction(d);
            if (a ? !a->isChecked() : d->isHidden()) continue;
            /* isSelectedDockTab, NOT !isHidden(): a back tab is only hidden once Qt has
               actually built the tab bar, so on a freshly restored layout every member
               of a group still reports not-hidden and "the front tab" came out as
               whichever happened to be last in findChildren order -- which is why
               re-opening a group the user had never clicked in came back on Bookmarks.
               isSelectedDockTab asks whether the dock has a non-empty visible region,
               which is true of exactly the one on top, and is what frontDockTabs()
               already uses for the per-workspace tab memory. */
            const bool current = isSelectedDockTab(d);
            if (current) extent = qMax(extent, horizontal ? d->width() : d->height());
            wanted << CollapsedDock{d, current};
        }
        /* Nothing the user wants here: do not record an empty collapse, or the bar
           would flip to "restore" with nothing to restore. */
        if (wanted.isEmpty()) return;
        areaCollapsed.insert(area, wanted);
        areaCollapsedExtent.insert(area, extent);
        for (const CollapsedDock &c : wanted)
            if (c.dock) c.dock->setVisible(false);
    }
    syncShowHideBars();
}

void MW::syncShowHideBars()
{
/*
    The single place that decides each bar's triangle and whether the bar is on screen:

        shown = this bar is holding the area collapsed          (it is the only way back)
             || the area holds a panel the user has enabled     (there is something to hide)

    The first clause is what keeps a collapsed side recoverable. Without it, collapsing
    would empty the area, the bar would judge itself pointless and vanish, and the only
    route back would be the F-keys -- the bar could hide panels but never show them.
*/
    if (!leftBarDock || !rightBarDock || !bottomBarDock || !topBarDock) return;

    struct Entry { Qt::DockWidgetArea area; ShowHideBar *bar; QToolBar *tb; };
    const Entry entries[] = {
        {Qt::LeftDockWidgetArea,   leftBar,   leftBarDock},
        {Qt::RightDockWidgetArea,  rightBar,  rightBarDock},
        {Qt::BottomDockWidgetArea, bottomBar, bottomBarDock},
        {Qt::TopDockWidgetArea,    topBar,    topBarDock},
    };

    for (const Entry &e : entries) {
        const bool collapsed = areaCollapsed.contains(e.area);
        bool hasPanel = collapsed;
        /* !isHidden() for the same reason as in toggleDockArea: this runs during the
           startup workspace, before the main window is shown, and isVisible() there is
           false for every dock -- which read as "no panels anywhere" and took all three
           bars off screen for the rest of the session. */
        if (!hasPanel)
            for (DockWidget *d : docksInArea(e.area))
                if (!d->isHidden()) { hasPanel = true; break; }
        e.bar->setExpanded(!collapsed);
        e.tb->setVisible(hasPanel && !isFullScreen());
    }
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
    apply(catalogDock,  "CatalogDock");
    apply(keywordsDock, "KeywordsDock");
    apply(metadataDock, "MetadataDock");
    apply(thumbDock,    "ThumbDock");
    apply(embelDock,    "EmbelDock");
    apply(historyDock,  "HistoryDock");
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

/*  PER-WORKSPACE TAB SELECTION ***************************************************************

    A workspace state blob carries the front tab of every tab group as it was when the
    layout was CAPTURED, so applying a workspace re-raises that panel -- Folders in the
    shipped Source layout -- and throws away whatever panel the user was actually working
    in.  Leaving Source on Keywords, pressing D and pressing E came back to Folders.

    Instead the front tab of each group is remembered per workspace when a workspace is
    left (MW::invokeWorkspace calls rememberDockTabSelection before it applies the new
    layout) and re-raised when that workspace is applied again.  A workspace not visited
    yet this session has nothing remembered, so it keeps the layout's own choice -- that
    is what makes the first D still open on the Develop panel and the first K on Keywords.

    Session only, and keyed on the workspace name and the dock objectNames, so a renamed
    workspace or a dock that has since left the group simply falls back to the layout.
*/

QStringList MW::frontDockTabs()
{
    QStringList front;
    QSet<QDockWidget*> seen;
    const QList<QDockWidget*> docks = findChildren<QDockWidget*>();
    for (QDockWidget *dock : docks) {
        if (seen.contains(dock)) continue;
        seen.insert(dock);
        QList<QDockWidget*> group = tabifiedDockWidgets(dock);
        if (group.isEmpty()) continue;          // not tabbed: nothing to choose
        for (QDockWidget *d : group) seen.insert(d);
        group.prepend(dock);
        for (QDockWidget *d : group) {
            if (isSelectedDockTab(d)) {
                front << d->objectName();
                break;
            }
        }
    }
    return front;
}

void MW::rememberDockTabSelection(const QString &wsName)
{
    if (G::isLogger) G::log("MW::rememberDockTabSelection", wsName);
    if (wsName.isEmpty()) return;
    const QStringList front = frontDockTabs();
    if (front.isEmpty()) return;
    frontDockTabsByWorkspace.insert(wsName, front);
}

void MW::restoreDockTabSelection(const QString &wsName)
{
/*
    Raise the panel last used in each tab group in this workspace.  Called after
    restoreState, which has already rebuilt the groups.
*/
    if (G::isLogger) G::log("MW::restoreDockTabSelection", wsName);
    if (wsName.isEmpty()) return;
    if (!frontDockTabsByWorkspace.contains(wsName)) return;
    const QStringList front = frontDockTabsByWorkspace.value(wsName);

    QSet<QDockWidget*> seen;
    const QList<QDockWidget*> docks = findChildren<QDockWidget*>();
    for (QDockWidget *dock : docks) {
        if (seen.contains(dock)) continue;
        seen.insert(dock);
        QList<QDockWidget*> group = tabifiedDockWidgets(dock);
        if (group.isEmpty()) continue;
        for (QDockWidget *d : group) seen.insert(d);
        group.prepend(dock);
        for (QDockWidget *d : group) {
            if (!d->isVisible()) continue;      // a hidden dock has no tab
            if (!front.contains(d->objectName())) continue;
            d->raise();
            break;
        }
    }
}
