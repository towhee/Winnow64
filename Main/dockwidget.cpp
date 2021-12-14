#include "dockwidget.h"

/* DockTitleBtn *******************************************************************************
A tool button that is placed in the DockTitleBar.  The styling is inherited from DockTitleBar,
which has a border, and must be over-ridden.
*/

BarBtn::BarBtn(/*QWidget *parent*/) : QToolButton()
{
    setStyleSheet(G::css);
//    setStyleSheet
//    (
//    "QToolTip {"
//        "background:yellow;"
//    "}"
//    );
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

DockTitleBar::DockTitleBar(const QString &title, QHBoxLayout *titleBarLayout) : QFrame()
{
    setStyle();
    setLayout(titleBarLayout);
    titleLabel = new QLabel;
    setTitle(title);
    titleLabel->setText(title);
    titleLabel->setStyleSheet("border:none;");
    titleBarLayout->addWidget(titleLabel);
    titleBarLayout->addStretch();
}

void DockTitleBar::setTitle(QString title)
{
    titleLabel->setText(title);
}

void DockTitleBar::setStyle()
{
    int g1 = G::backgroundShade;
    int g0 = g1 - 10;
    if (g0 < 0) g0 = 0;
    int c = G::backgroundShade + 30;
    QString s = "background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,"
                "stop: 0 " + QColor(g1,g1,g1).name() + ", "
                "stop: 1 " + QColor(g0,g0,g0).name() + ");"
                "padding-left: 2px;"
                "padding-bottom: 2px;"
                "border: none;"
                "font-size:" + G::fontSize + "pt;";
    setStyleSheet(s);
}

/* DockWidget *********************************************************************************
QDockWidget has a feature where you can double click on the title bar and the dock will toggle
to a floating window and back to its docked state. The problem is if you move and resize the
floating window and then toggle back to the dock and then back to floating again, your
position and size are lost.

This subclass of QDockWidget overrides the MouseButtonDblClick, resizeEvent and moveEvent,
ignoring Qt's attempts to impose its size and location "suggestions". The screen, position and
window size are stored in a struct, which in turn is saved in QSettings for persistence
between sessions.

When a mouse double click occurs in the docked state, the stored screen, postion and size are
used to re-establish the prior state.

I have not figured out how to draw a border around the DockWidget, so the contained treeview
stylesheet is used instead, along with a border around the DockTitleBar.

Open Winnow: thumb dock floating
    DockWidget::moveEvent QMoveEvent(0,276, non-spontaneous) ignore = false isFloating = false
    DockWidget::resizeEvent QResizeEvent(275, 264, non-spontaneous) isInitializing = true isFloating = false dw.size = QSize(694, 541) dw.pos = QPoint(339,368)
    DockWidget::showEvent QShowEvent(Show, 0x82e62fde38) isInitializing = true isFloating = false dw.size = QSize(694, 541) dw.pos = QPoint(339,368)
    DockWidget::moveEvent QMoveEvent(0,576, non-spontaneous) ignore = false isFloating = false
    DockWidget::resizeEvent QResizeEvent(275, 599, non-spontaneous) isInitializing = true isFloating = false dw.size = QSize(694, 541) dw.pos = QPoint(339,368)
    Floating = true: moveEvent, resizeEvent, showEvent
    DockWidget::moveEvent QMoveEvent(339,348, non-spontaneous) ignore = false isFloating = true
    DockWidget::moveEvent Redefine dw.pos: QPoint(339,348)
    DockWidget::resizeEvent QResizeEvent(694, 539, non-spontaneous) isInitializing = true isFloating = true dw.size = QSize(694, 539) dw.pos = QPoint(339,348)
    DockWidget::resizeEvent QResizeEvent(694, 539, non-spontaneous) Define dw: dw.size = QSize(694, 539) dw.pos = QPoint(339,368) dw.devicePixelRatio  1
    DockWidget::showEvent QShowEvent(Show, 0x82e62fc338) isInitializing = true isFloating = true dw.size = QSize(694, 539) dw.pos = QPoint(339,368)

Double mouse click on docked thumbDock
    DockWidget::event QMouseEvent(MouseButtonDblClick, LeftButton, localPos=9,138, screenPos=4052,1306) isFloating = false
    DockWidget::moveEvent QMoveEvent(2253,712, non-spontaneous) ignore = true isFloating = true spontaneous = false dw.size = QSize(1033, 894) dw.pos = QPoint(2253,712)
    DockWidget::resizeEvent QResizeEvent(1033, 286, non-spontaneous) isInitializing = false ignore = true isFloating = true spontaneous = false dw.size = QSize(1033, 894) dw.pos = QPoint(2253,712)
    IconView::resizeEvent m2->thumbDock->isFloating() = true
    DockWidget::showEvent QShowEvent(Show, 0x43878fa410) isInitializing = false isFloating = true dw.size = QSize(1033, 894) dw.pos = QPoint(2253,712)
    DockWidget::sizeHint dw.size = QSize(1033, 894)
    IconView::resizeEvent
    MW::setThumbDockFloatFeatures                      isFloat = 1
    IconView::resizeEvent m2->thumbDock->isFloating() = true
    DockWidget::event resizing to QSize(1033, 894)
    DockWidget::sizeHint dw.size = QSize(1033, 894)
    MW::setThumbDockFeatures
    IconView::resizeEvent m2->thumbDock->isFloating() = true
    DockWidget::resizeEvent QResizeEvent(1033, 894, non-spontaneous) isInitializing = false ignore = false isFloating = true spontaneous = false dw.size = QSize(1033, 894) dw.pos = QPoint(2253,712)
    IconView::resizeEvent

QWindowsWindow::setGeometry: Unable to set geometry 1157x200-2495+472 (frame: 1179x256-2506+427)
on QWidgetWindow/"thumbDockWindow" on "\\.\DISPLAY5". Resulting geometry: 1738x303-2492+486 (frame: 1760x359-2503+441)
margins: 11, 45, 11, 11
minimum size: 70x70
maximum size: 524287x286
MINMAXINFO maxSize=0,0 maxpos=0,0 mintrack=127,161 maxtrack=786453,485)
*/

DockWidget::DockWidget(const QString &title, QWidget *parent)
    : QDockWidget(title, parent)
{
    ignoreResize = false;
}

bool DockWidget::event(QEvent *event)
{
    if (event->type() == QEvent::MouseButtonDblClick && objectName() == "thumbDock") {
        qDebug() << __FUNCTION__ << event
                 << "isFloating =" << isFloating()
                    ;
        ignoreResize = true;
        setFloating(!isFloating());
        if (isFloating()) {
            move(dw.pos);
            adjustSize();
        }
        ignoreResize = false;
        return true;
    }

    QDockWidget::event(event);
    return true;
}

void DockWidget::showEvent(QShowEvent *event)
{
    if (objectName() == "thumbDock")
        qDebug() << __FUNCTION__ << event
                 << "isInitializing =" << isInitializing
                 << "isFloating =" << isFloating()
                 << "dw.size =" << dw.size
                 << "dw.pos =" << dw.pos
                    ;
    if (isInitializing && isFloating()) {
        dw.size /= dw.devicePixelRatio;
        adjustSize();
        isInitializing = false;
    }
    QDockWidget::showEvent(event);
}

QSize DockWidget::sizeHint() const
{
    if (objectName() == "thumbDock") {
        qDebug() << __FUNCTION__ << "dw.size =" << dw.size;
        return dw.size;
    }
    return QDockWidget::sizeHint();
}

void DockWidget::resizeEvent(QResizeEvent *event)
{
    if (objectName() == "thumbDock")
        qDebug() << __FUNCTION__ << event
                 << "isInitializing =" << isInitializing
                 << "ignoreResize =" << ignoreResize
                 << "isFloating =" << isFloating()
                 << "spontaneous =" << event->spontaneous()
                 << "dw.size =" << dw.size
                 << "dw.pos =" << dw.pos
                    ;
    if (ignoreResize) return;

    if (isFloating() && event->spontaneous() && objectName() == "thumbDock") {
        dw.screen = QApplication::desktop()->screenNumber(this);
        dw.devicePixelRatio =  QGuiApplication::screens().at(dw.screen)->devicePixelRatio();
        dw.size = event->size() /*/ dw.devicePixelRatio*/;
        qDebug() << __FUNCTION__ << event << "Define dw:"
                 << "dw.size =" << dw.size
                 << "dw.pos =" << dw.pos
                 << "dw.devicePixelRatio " << dw.devicePixelRatio
                    ;
    }
    QDockWidget::resizeEvent(event);
}

void DockWidget::moveEvent(QMoveEvent *event)
{
    if (objectName() == "thumbDock")
        qDebug() << __FUNCTION__ << event
                 << "ignore =" << ignoreResize
                 << "isFloating =" << isFloating()
                 << "spontaneous =" << event->spontaneous()
                 << "dw.size =" << dw.size
                 << "dw.pos =" << dw.pos
                    ;
//    if (ignore || !isFloating()) return;
    if (isFloating() && event->spontaneous() && objectName() == "thumbDock") {
        dw.screen = QApplication::desktop()->screenNumber(this);
        QRect r = frameGeometry();
        QRect a = QGuiApplication::screens().at(dw.screen)->geometry();
        dw.pos = QPoint(r.x(), r.y());
//        dw.pos = QPoint(r.x() - a.x(), r.y() - a.y());
        qDebug() << __FUNCTION__ << "Redefine dw.pos:"
                 << "dw.pos =" << dw.pos
                 << "frameGeometry() =" << r
                 << "screenGeometry() =" << a
                    ;
//        QDockWidget::moveEvent(event);
        return;
    }
    QDockWidget::moveEvent(event);
}

void DockWidget::rpt(QString s)
{
    qDebug() << s
             << "isFloating" << isFloating()
             << "screen" << dw.screen
             << "pos" << dw.pos
             << "size" << dw.size;
    s = "";
}
