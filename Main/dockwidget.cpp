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
//                "border: 1px solid " + QColor(c,c,c).name() + ";"
//                "border-bottom: 0px;"
//                "border-right: 1px solid " + QColor(c,c,c).name() + ";"
//                "border-top:   1px solid " + QColor(c,c,c).name() + ";"
                "font-size:" + G::fontSize + "pt;";
    setStyleSheet(s);
}

/* DockWidget *********************************************************************************
QDockWidget has a feature where you can double click on the title bar and the dock will toggle
to a floating window and back to its docked state. The problem is if you move and resize the
floating window and then toggle back to the dock and then back to floating again, your
position and size are lost.

This subclass of QDockWidget overrides the MouseButtonDblClick, resizeEvent and moveEvent,
ignoring Qt's attempts to impose its size and location "suggestions. The screen, position and
window size and stored in a struct, which in turn is saved in QSettings for persistence
between sessions.

When a mouse double click occurs in the docked state, the stored screen, postion and size are
used to re-establish the prior state.

I have not figured out how to braw a border around the DockWidget, so the contained treeview
stylesheet is used instead, along wiht a border around the DockTitleBar.
*/

DockWidget::DockWidget(const QString &title, QWidget *parent)
    : QDockWidget(title, parent)
{
    ignore = false;
}

bool DockWidget::event(QEvent *event)
{
    if (event->type() == QEvent::MouseButtonDblClick) {
        ignore = true;
        setFloating(!isFloating());
        if (isFloating()) {
            // move and size to previous state
            QRect screenRect = QGuiApplication::screens().at(dw.screen)->geometry();
            move(QPoint(screenRect.x() + dw.pos.x(), screenRect.y() + dw.pos.y()));
            ignore = false;
            adjustSize();
        }
        ignore = false;
        return true;
    }
    QDockWidget::event(event);
    return true;
}

void DockWidget::resizeEvent(QResizeEvent *event)
{
    if (ignore) {
        return;
    }
    if (isFloating()) {
        dw.screen = QApplication::desktop()->screenNumber(this);
        QRect r = geometry();
        QRect a = QGuiApplication::screens().at(dw.screen)->geometry();
        dw.pos = QPoint(r.x() - a.x(), r.y() - a.y());
        dw.size = event->size();
    }
}

QSize DockWidget::sizeHint() const
{
    return dw.size;
}

void DockWidget::moveEvent(QMoveEvent *event)
{
    if (ignore || !isFloating()) return;
    dw.screen = QApplication::desktop()->screenNumber(this);
    QRect r = geometry();
    QRect a = QGuiApplication::screens().at(dw.screen)->geometry();
    dw.pos = QPoint(r.x() - a.x(), r.y() - a.y());
    dw.size = QSize(r.width(), r.height());
    QDockWidget::moveEvent(event);  // suppress compiler warning
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
