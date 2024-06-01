#include "dockwidget.h"

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
    QString s = "padding-left: 2px;"
                "border: none;"
                "color: #6CC1E8;"
                "font-size:" + G::strFontSize + "pt;";
    setStyleSheet(s);
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


/* DockWidget *********************************************************************************

    QDockWidget has a feature where you can double click on the title bar and the dock
    will toggle to a floating window and back to its docked state. The problem is if you
    move and resize the floating window and then toggle back to the dock and then back to
    floating again, your position and size are lost.

    This subclass of QDockWidget overrides the MouseButtonDblClick, resizeEvent and
    moveEvent, ignoring Qt's attempts to impose its size and location "suggestions". The
    DockWidget geometry is saved in QSettings for persistence between sessions.

    When a mouse double click occurs in the docked state, the stored geometry is used to
    re-establish the prior state, factoring in any changes to the screen scale.

    If the DockWidget does not have a custom TitleBarWidget (ie thumbDock), then the
    mouse double click event is not triggered. The restore() function is called from
    MW::setThumbDockFloatFeatures. The save() function is triggered from the
    DockWidget::moveEvent and DockWidget::resizeEvent events.

    I have not figured out how to draw a border around the DockWidget, so the contained
    treeview stylesheet is used instead, along with a border around the DockTitleBar.

*/

DockWidget::DockWidget(const QString &title, QString objName, QWidget *parent)
    : QDockWidget(title, parent)
{
    setObjectName(objName);
    isRestoring = false;
}

void DockWidget::listAllChildren()
{
    // Function to list all children of the QDockWidget
    const QObjectList &children = this->children();
    for (QObject *child : children) {
        QWidget *widget = qobject_cast<QWidget *>(child);
        if (widget) {
            qDebug() << "Class name:" << widget->metaObject()->className() << ", Object name:" << widget->objectName();
        } else {
            qDebug() << "Class name:" << child->metaObject()->className() << ", Object name:" << child->objectName();
        }
    }
}

bool DockWidget::hasCustomTitleBar()
{
    QWidget *titleBarWidget = this->titleBarWidget();
    if (titleBarWidget) return true;
    else return false;
}

// void DockWidget::showEvent(QShowEvent *event)
// {
//     QDockWidget::showEvent(event);

//     // // QString defaultTitleWidgetName = "qt_dockwidget_title"; // nada
//     // QString defaultTitleWidgetName = "qt_scrollarea_hcontainer";

//     // // Install the event filter on the default title bar widget
//     // QWidget *titleBarWidget = this->titleBarWidget();
//     // if (titleBarWidget) {
//     //     titleBarWidget->installEventFilter(this);
//     // } else {
//     //     // If titleBarWidget is not explicitly set, retrieve the default title bar widget
//     //     QWidget *defaultTitleBarWidget = findChild<QWidget*>(defaultTitleWidgetName);
//     //     if (defaultTitleBarWidget) {
//     //         defaultTitleBarWidget->installEventFilter(this);
//     //     }
//     //     else qWarning() << "DockWidget::showEvent failed to install eventFilter" << objectName();
//     // }
// }

// bool DockWidget::eventFilter(QObject *watched, QEvent *event)
// {
//     // qDebug() << "DockWidget::eventFilter"
//     //          << "watched =" << watched  << watched->objectName()
//     //          << event;
//     if (/*watched == titleBarWidget() &&*/ event->type() == QEvent::MouseButtonDblClick) {
//         QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
//         qDebug() << "DockWidget title bar double-clicked at position:" << mouseEvent->pos();

//         // Handle the double-click event as needed
//         // ...

//         return true; // Event handled
//     }
//     return QDockWidget::eventFilter(watched, event);
// }

bool DockWidget::event(QEvent *event)
{
/*
    QDockWidget title bar overrides MouseButtonDblClick and does not propogate the event
    so it cannot be captured in DockWidget.  Instead capture here.
*/
    // QDockWidget::event(event);
    // return true;
    // qDebug() << "DockWidget::event" << event << objectName()
    //          << "isFloating =" << isFloating();

    if (event->type() == QEvent::NonClientAreaMouseButtonDblClick) {
        // qDebug() << "DockWidget::event" << event << objectName()
        //          << "isFloating =" << isFloating();
        save();
        setFloating(false);
        emit focus(this);
        return true;
    }
    if (event->type() == QEvent::MouseButtonDblClick) {
        if (G::isLogger) G::log("DockWidget::event", "QEvent::MouseButtonDblClick " + objectName());
        // qDebug() << "DockWidget::event" << event << objectName()
        //          << "isFloating =" << isFloating();
        // qDebug() << "DockWidget::event" << event << objectName()
        //          << "isFloating =" << isFloating();
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

    // if (event->type() == QEvent::ShowToParent) {
    //     qDebug() << "DockWidget::event" << event << objectName()
    //              << "isFloating =" << isFloating();
    // }

    // if (event->type() == QEvent::ActivationChange) {
    //     qDebug() << "DockWidget::event" << event << objectName()
    //              << "isFloating =" << isFloating();
    //     // if (isRestoring) setGeometry(floatingGeometry);
    //     isRestoring = false;
    // }

    QDockWidget::event(event);
    return true;
}

// void DockWidget::resizeEvent(QResizeEvent *event)
// {
// /*
//     If the DockWidget does not have a custom TitleBarWidget (ie thumbDock) and the
//     DockWidget is floating, then the mouse double click event is not triggered and the
//     save() function is not invoked when the floating DockWidget is docked via double
//     click by the parent QDockWidget. As a failsafe, the save() function is invoked here,
//     unless the event was triggered by a double click on the docked titlebar, which does
//     trigger the mouse double click event.
// */
//     qDebug() << "DockWidget::resizeEvent" << event
//              << "isFloating() =" << isFloating()
//              << "doubleClickDocked =" << doubleClickDocked
//     ;

//     if (doubleClickDocked) {
//         doubleClickDocked = false;
//         return;
//     }
//     if (isFloating()) {
//         // qDebug() << "DockWidget::resizeEvent isFloating() = true";
//         // save();
//     }
//     QDockWidget::resizeEvent(event);
// }

// void DockWidget::moveEvent(QMoveEvent *event)
// {
// /*
//     If the DockWidget does not have a custom TitleBarWidget (ie thumbDock) and the
//     DockWidget is floating, then the mouse double click event is not triggered and the
//     save() function is not invoked when the floating DockWidget is docked via double
//     click by the parent QDockWidget. As a failsafe, the save() function is invoked here,
//     unless the event was triggered by a double click on the docked titlebar, which does
//     trigger the mouse double click event.
// */
//     // if (doubleClickDocked) {
//     //     doubleClickDocked = false;
//     //     return;
//     // }
//     if (isFloating()) {
//         // qDebug() << "DockWidget::moveEvent isFloating() = true";
//         // save();
//     }
//     QDockWidget::moveEvent(event);
// }

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
}

void DockWidget::save()
{
    if (G::isLogger) G::log("DockWidget::save", objectName());
    qDebug() << "DockWidget::save" << objectName() << geometry();
    G::settings->beginGroup((objectName()));
    G::settings->setValue("geometry", frameGeometry());
    // G::settings->setValue("geometry", geometry());
    G::settings->endGroup();
    // rpt("Save");
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
    G::wait(10);
    setGeometry(floatingGeometry);
    // rpt("Restore");
}

QRect DockWidget::deconstructSavedGeometry(QByteArray geometry)
{
/*
    Qwidget::restoreGeometry(const QByteArray &geometry)
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
