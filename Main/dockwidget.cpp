#include "dockwidget.h"

/*
// RichTextTabBar *****************************************************************************
//    Not being used.  Would require also subclassing QMainWindow to use this.

void RichTextTabBar::setTabText(int index, const QString& text)
{
    QLabel* label = new QLabel(text);
    label->setFixedSize(mTabWidth, mTabHeight);
    label->setStyleSheet("padding-top:0px; padding-bottom:0px; padding-left:5px; padding-right:5px;");
    setTabButton(index, QTabBar::LeftSide, label);
}

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
    int h = style()->pixelMetric(QStyle::PM_TitleBarHeight) * 1.1;
    return QSize(width(), h);
}

DockTitleBar::DockTitleBar(const QString &title, QHBoxLayout *titleBarLayout) : QWidget()
{
    setStyle();
    setLayout(titleBarLayout);
    titleLabel = new QLabel;
    titleLabel->setTextFormat(Qt::RichText);
    setTitle(title);
    titleLabel->setText(title);
    //titleLabel->setPixmap(QPixmap(":/images/icon16/anchor.png"));
    titleLabel->setStyleSheet("border:none;");
    titleBarLayout->addWidget(titleLabel);
    titleBarLayout->addStretch();
}

void DockTitleBar::setTitle(QString title)
{
    titleLabel->setText(title);
    //titleLabel->setPixmap(QPixmap(":/images/icon16/anchor.png"));
}

void DockTitleBar::setStyle()
{
    int g1 = G::backgroundShade;
    int g0 = g1 - 10;
    if (g0 < 0) g0 = 0;
//    int c = G::backgroundShade + 30;
    QString s = "background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,"
                "stop: 0 " + QColor(g1,g1,g1).name() + ", "
                "stop: 1 " + QColor(g0,g0,g0).name() + ");"
                "padding-left: 2px;"
//                "padding-bottom: 2px;"
                "border: none;"
                "font-size:" + G::strFontSize + "pt;";
    setStyleSheet(s);
}

/* DockWidget *********************************************************************************

QDockWidget has a feature where you can double click on the title bar and the dock will toggle
to a floating window and back to its docked state. The problem is if you move and resize the
floating window and then toggle back to the dock and then back to floating again, your
position and size are lost.

This subclass of QDockWidget overrides the MouseButtonDblClick, resizeEvent and moveEvent,
ignoring Qt's attempts to impose its size and location "suggestions". The screen, position,
window size and screen devicePixelRatio are stored in a struct dw, which in turn is saved in
QSettings for persistence between sessions.

When a mouse double click occurs in the docked state, the stored screen, postion and size are
used to re-establish the prior state, factoring in any changes to the screen scale.

I have not figured out how to draw a border around the DockWidget, so the contained treeview
stylesheet is used instead, along with a border around the DockTitleBar.

*/

DockWidget::DockWidget(const QString &title, QWidget *parent)
    : QDockWidget(title, parent)
{
    ignoreResize = false;
}

bool DockWidget::event(QEvent *event)
{
    if (event->type() == QEvent::MouseButtonDblClick) {
        if (G::isLogger) G::log("DockWidget::DockWidget::");
        /*
        qDebug() << "DockWidget::DockWidget::" << event << objectName()
                 << "isFloating =" << isFloating()
                 << "dw.devicePixelRatio " << dw.devicePixelRatio
                 << "prevDpr " << prevDpr
                    ;
                    //*/
        ignoreResize = true;
        setFloating(!isFloating());
        if (isFloating()) {
            move(dw.pos);
            dw.devicePixelRatio =  dw.devicePixelRatio =  this->screen()->devicePixelRatio();
            //dw.devicePixelRatio =  QGuiApplication::screens().at(dw.screen)->devicePixelRatio();
            if (!qFuzzyCompare(dw.devicePixelRatio, prevDpr)) {
                dprSize *= prevDpr;
                dprSize /= dw.devicePixelRatio;
            }
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
    if (G::isLogger) G::log("DockWidget::showEvent", objectName());
    /*
    if (isFloating())
        qDebug() << "DockWidget::showEvent" << event << objectName()
                 << "isInitializing =" << isInitializing
                 << "isFloating =" << isFloating()
                 << "dprSize =" << dprSize
                 << "dw.size =" << dw.size
                 << "dw.pos =" << dw.pos
                 << "dw.devicePixelRatio " << dw.devicePixelRatio
                 << "prevDpr " << prevDpr
                    ;
                    //*/
    if (isInitializing && isFloating()) {
        dprSize = dw.size / dw.devicePixelRatio;
        adjustSize();
        isInitializing = false;
    }
    QDockWidget::showEvent(event);
}

QSize DockWidget::sizeHint() const
{
    if (G::isLogger) G::log("DockWidget::sizeHint");
    if (isFloating()) {
        /*
        qDebug() << "DockWidget::sizeHint" << "dprSize =" << dprSize;
        //*/
        return dprSize;
    }
    return QDockWidget::sizeHint();
}

void DockWidget::resizeEvent(QResizeEvent *event)
{
    if (G::isLogger) G::log("DockWidget::resizeEvent");
    /*
    if (isFloating())
        qDebug() << "DockWidget::resizeEvent" << event << objectName()
                 << "isInitializing =" << isInitializing
                 << "ignoreResize =" << ignoreResize
                 << "isFloating =" << isFloating()
                 << "spontaneous =" << event->spontaneous()
                 << "dprSize =" << dprSize
                 << "dw.size =" << dw.size
                 << "dw.pos =" << dw.pos
                 << "dw.devicePixelRatio " << dw.devicePixelRatio
                 << "prevDpr " << prevDpr
                    ;
                    //*/
    if (isInitializing) {
        prevDpr = dw.devicePixelRatio;
    }

    if (ignoreResize) return;

    if (isFloating()) {
        //dw.screen = QApplication::desktop()->screenNumber(this);
        prevDpr = dw.devicePixelRatio;
        dw.devicePixelRatio =  this->screen()->devicePixelRatio();
        //dw.devicePixelRatio =  QGuiApplication::screens().at(dw.screen)->devicePixelRatio();
        dw.size = event->size();
        dprSize = dw.size;
        /*
        qDebug() << "DockWidget::resizeEvent" << event << "Define dw:"
                 << "dprSize =" << dprSize
                 << "dw.size =" << dw.size
                 << "dw.devicePixelRatio " << dw.devicePixelRatio
                 << "prevDpr " << prevDpr
                    ;
                    //*/
    }
    QDockWidget::resizeEvent(event);
}

void DockWidget::moveEvent(QMoveEvent *event)
{
    if (G::isLogger) G::log("DockWidget::moveEvent", objectName());
    /*
    if (isFloating())
        qDebug() << "DockWidget::moveEvent" << event << objectName()
                 << "ignore =" << ignoreResize
                 << "isFloating =" << isFloating()
                 << "spontaneous =" << event->spontaneous()
                 << "dw.size =" << dw.size
                 << "dw.pos =" << dw.pos
                 << "dw.devicePixelRatio " << dw.devicePixelRatio
                 << "prevDpr " << prevDpr
                    ;
                    //*/
    if (isFloating()) {
        //dw.screen = QApplication::desktop()->screenNumber(this);
        dw.pos = QPoint(frameGeometry().x(), frameGeometry().y());
        /*
        qDebug() << "DockWidget::moveEvent" << "Redefine dw.pos:"
                 << "dw.pos =" << dw.pos
                 << "frameGeometry() =" << frameGeometry()
                 << "dw.devicePixelRatio " << dw.devicePixelRatio
                 << "prevDpr " << prevDpr
                    ;
                    //*/
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
