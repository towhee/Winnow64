#include "zoomdlg.h"
#include "ui_zoomdlg.h"
#include "QDebug"

ZoomDlg::ZoomDlg(QWidget *parent, qreal zoom, QRect a, QRect c) : QDialog(parent),
    ui(new Ui::ZoomDlg)
{
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::Dialog | Qt::FramelessWindowHint);
    ui->setupUi(this);

    // Toggle zoom dialog off (MW shortcuts inactive while ZoomDlg has focus)
    QAction *okayAction = new QAction(tr("Okay"), this);
    okayAction->setShortcut(QKeySequence("Return"));
    this->addAction(okayAction);
    connect(okayAction, SIGNAL(triggered(bool)), this, SLOT(closeZoomDlg()));

    QAction *okay2Action = new QAction(tr("Okay"), this);
    okay2Action->setShortcut(QKeySequence("Z"));
    this->addAction(okay2Action);
    connect(okay2Action, SIGNAL(triggered(bool)), this, SLOT(closeZoomDlg()));

    // update controls to current zoom factor
    zoomChange(zoom);

    // do some formatting (color: yellow; font-size: 18px;) NOTE keep in sync with winnow.css
    QString spinBoxStyle = "QSpinBox {color: yellow; font-size: 18px; background-color: rgb(111,111,111); border: 1px solid gray; border-radius: 5px; padding-left: 4px;}QSpinBox:hover, QSpinBox:focus {border-color: silver;}QSpinBox:disabled {color: rgb(77,77,77);background-color: rgb(88,88,88);}QSpinBox::up-button, QSpinBox::down-button  {width: 0px;border-width: 0px;}";
    ui->zoomSB->setStyleSheet(spinBoxStyle);
    QString labelStyle = "QLabel {color: yellow; font-size: 18px; border:none;}";
    ui->pctLabel->setStyleSheet(labelStyle);
    ui->border->setStyleSheet("QFrame {border: 2px solid rgb(125,125,125); border-radius: 4px;}");

    // position in middle of main window and at the bottom of the central widget
    positionWindow(a, c);
}

ZoomDlg::~ZoomDlg()
{
    delete ui;
}

void ZoomDlg::closeZoomDlg()
{
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
    qDebug() << __PRETTY_FUNCTION__ ;
    if (isVisible()) emit closeZoom();
}

void ZoomDlg::positionWindow(QRect a, QRect c)
{
/*
This function positions the zoom dialog in the lower center of the central widget
*/
    if (G::isLogger) G::log(__PRETTY_FUNCTION__); 
    // a = mainWindow->geometry()
    // c = centralWidget->geometry();
    int w = width();        // width of this dialog
    int h = height();       // height of this dialog
    int x;                  // top left coord of the zoom dialog
    int y;                  // top left coord of the zoom dialog
    // bottom center of the central widget
    x = a.left() + c.left() + c.width() / 2;
    y = a.top() + c.bottom();
    // adjust for zoom dialog size
    x -= w / 2;
    y -= h;
    setGeometry(x, y, w, h);
//    // anchor point = center of app and bottom central widget
//    QPoint anchor = a.topLeft() + QPoint(0, c.height()) + QPoint(a.width() / 2, 0);
//    setGeometry(anchor.x() - w/2, anchor.y() - h, w, h);
}

void ZoomDlg::zoomChange(qreal zoom)
{
/*
This slot receives signals from either imageView or compareView when the scale
changes and the scale dialog (this) is showing.  The zoom slider and spinbox
controls are updated to the current scale.
*/
    if (G::isLogger) G::log(__PRETTY_FUNCTION__); 
//    qDebug() << "ZoomDlg::zoomChange" << zoom << G::actDevicePixelRatio;
    // convert to percentage for persentation
    zoom *= 100;        //(100 * G::actDevicePixelRatio);

    // update controls
    ui->zoomSB->setValue(qRound(zoom));
    if (zoom >= 1 && zoom <= 800) ui->zoomSlider->setValue(qRound(zoom));
}

void ZoomDlg::on_zoomSB_valueChanged(int value)
{
/*
    When the zoom spinbox value changes a signal is emitted to the slots in imageView
    and compareView.

    Check for circular messaging where an external change of scale was signalled
    to ZoomDlg, which updated this control, and then signals back, by limiting signals
    to only changes made lacally via the isActiveWindow flag.
*/
    if (G::isLogger) G::log(__PRETTY_FUNCTION__); 
    if (isActiveWindow()) emit zoom((qreal)value / 100/* / G::actDevicePixelRatio*/);
}

void ZoomDlg::on_toggleZoomAmountBtn_clicked()
{
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
    QString msg = "Toggle zoom amount changed to " +
                  QString::number(ui->zoomSB->value()) + "%";
    G::popUp->showPopup(msg, 1500);
    qreal zoomVal = (qreal)ui->zoomSB->value() / 100/* / G::actDevicePixelRatio*/;
    emit updateToggleZoom(zoomVal);
//    QDialog::accept();
}

// radio buttons for quick zoom setting at popular scales
void ZoomDlg::on_radio25Button_clicked()
{
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
    emit zoom(0.25 / G::actDevicePixelRatio);
}

void ZoomDlg::on_radio50Button_clicked()
{
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
    emit zoom(0.50 /*/ G::actDevicePixelRatio*/);
}

void ZoomDlg::on_radio66Button_clicked()
{
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
    emit zoom(0.666667/* / G::actDevicePixelRatio*/);
}

void ZoomDlg::on_radio100Button_clicked()
{
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
    emit zoom(1.0/* / G::actDevicePixelRatio*/);
}

void ZoomDlg::on_radio133Button_clicked()
{
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
    emit zoom(1.33333333/* / G::actDevicePixelRatio*/);
}

void ZoomDlg::on_radio200Button_clicked()
{
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
    emit zoom(2.0/* / G::actDevicePixelRatio*/);
}

void ZoomDlg::enterEvent(QEvent* /*event*/)
{
/*
    For convenience, set window focus on mouseover
*/
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
    this->activateWindow();
    this->setFocus();
    this->ui->zoomSlider->setFocus();
}

void ZoomDlg::leaveEvent(QEvent* /*event*/)
{
    /*
    For convenience, set window focus on mouseover
*/
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
    emit leaveZoom();
}

void ZoomDlg::changeEvent(QEvent *event)
{
/*
    Set some formatting to make it clear when the ZoomDlg has focus
*/
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
    QWidget::changeEvent(event);
    if (event->type() == QEvent::ActivationChange)
    {
        if(this->isActiveWindow())
        {
            ui->border->setStyleSheet("QFrame {border: 2px solid rgb(125,125,125); border-radius: 4px;}");
            setWindowOpacity(0.85);
        }
        else
        {
            // widget is now inactive
            ui->border->setStyleSheet("QFrame {border: 2px solid rgb(85,85,85); border-radius: 4px;}");
            setWindowOpacity(0.50);
        }
    }
    QDialog::changeEvent(event);
}
