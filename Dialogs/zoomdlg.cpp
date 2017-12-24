#include "zoomdlg.h"
#include "ui_zoomdlg.h"
#include "QDebug"

ZoomDlg::ZoomDlg(QWidget *parent, qreal zoom, QRect a, QRect c) : QDialog(parent),
    ui(new Ui::ZoomDlg)
{
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::Dialog | Qt::FramelessWindowHint);
    ui->setupUi(this);

    // use to visually confirm changes to toggle zoom amount
    popUp = new PopUp;

    // shortcuts
    QAction *okayAction = new QAction(tr("Okay"), this);
    okayAction->setShortcut(QKeySequence("Return"));
    this->addAction(okayAction);
    connect(okayAction, SIGNAL(triggered(bool)), this, SLOT(accept()));

    QAction *okay2Action = new QAction(tr("Okay"), this);
    okay2Action->setShortcut(QKeySequence("Z"));
    this->addAction(okay2Action);
    connect(okay2Action, SIGNAL(triggered(bool)), this, SLOT(accept()));

    // update controls to current zoom factor
    zoomChange(zoom);

    // do some formatting (color: yellow; font-size: 18px;) NOTE keep in sync with winnow.css
    QString spinBoxStyle = "QSpinBox {color: yellow; font-size: 18px; background-color: rgb(111,111,111); border: 1px solid gray; selection-background-color: darkgray; border-radius: 5px; padding-left: 4px;}QSpinBox:hover, QSpinBox:focus {border-color: silver;}QSpinBox:disabled {color: rgb(77,77,77);background-color: rgb(88,88,88);}QSpinBox::up-button, QSpinBox::down-button  {width: 0px;border-width: 0px;}";
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

void ZoomDlg::close()
{
    accept();
}

void ZoomDlg::positionWindow(QRect a, QRect c)
{
    // a = mainWindow->geometry()
    // c = centralWidget->geometry();
    int w = width();        // width of this dialog
    int h = height();       // height of this dialog
    // anchor point = center of app and bottom central widget
    QPoint anchor = a.topLeft() + QPoint(0, c.height()) + QPoint(a.width() / 2, 0);
    setGeometry(anchor.x() - w/2, anchor.y() - h, w, h);
}

void ZoomDlg::zoomChange(qreal zoom)
{
/*
This slot receives signals from either imageView or compareView when the scale
changes and the scale dialog (this) is showing.  The zoom slider and spinbox
controls are updated to the current scale.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ZoomDlg::zoomChange" << zoom;
    #endif
    }
    qDebug() << "ZoomDlg::zoomChange" << zoom << G::actualDevicePixelRatio;
    // convert to percentage for persentation
    zoom *= (100 * G::actualDevicePixelRatio);

    // update controls
    ui->zoomSB->setValue(qRound(zoom));
    if (zoom >= 1 && zoom <= 200) ui->zoomSlider->setValue(qRound(zoom));
}

void ZoomDlg::on_zoomSB_valueChanged(int value)
{
/*
When the zoom spinbox value changes a signal is emited to the slots in imageView
and compareView.

Check for circular messaging where an external change of scale was signalled
to ZoomDlg, which updated this control, and then signals back, by limiting signals
to only changes made lacally via the isActiveWindow flag.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ZoomDlg::on_zoomSB_valueChanged" << value;
    #endif
    }
    if (isActiveWindow()) emit zoom((qreal)value / 100 / G::actualDevicePixelRatio);
}

void ZoomDlg::on_toggleZoomAmountBtn_clicked()
{
    QString msg = "Toggle zoom amount changed to " +
                  QString::number(ui->zoomSB->value()) + "%";
    popUp->showPopup(this, msg, 1500, 0.75);
    qreal zoomVal = (qreal)ui->zoomSB->value() / 100 / G::actualDevicePixelRatio;
    emit updateToggleZoom(zoomVal);
//    QDialog::accept();
}

// radio buttons for quick zoom setting at popular scales
void ZoomDlg::on_radio25Button_clicked()
{
    emit zoom(0.25);
}

void ZoomDlg::on_radio50Button_clicked()
{
    emit zoom(0.50);
}

void ZoomDlg::on_radio66Button_clicked()
{
    emit zoom(0.666667);
}

void ZoomDlg::on_radio100Button_clicked()
{
    emit zoom(1.0);
}

void ZoomDlg::on_radio133Button_clicked()
{
    emit zoom(1.33333333);
}

void ZoomDlg::on_radio200Button_clicked()
{
    emit zoom(2.0);
}

void ZoomDlg::enterEvent(QEvent *event)
{
/*
For convenience, set window focus on mouseover
*/
    {
    #ifdef ISDEBUG
//    qDebug() << "ZoomDlg::enterEvent" << currentImagePath;
    #endif
    }
    qDebug() << "ZoomDlg enter event";
    this->activateWindow();
    this->setFocus();
    this->ui->zoomSlider->setFocus();
}

void ZoomDlg::changeEvent(QEvent *event)
{
/*
Set some formatting to make it clear when the ZoomDlg has focus
*/
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
            qDebug() << "ZoomDlg lost focus";
            ui->border->setStyleSheet("QFrame {border: 2px solid rgb(85,85,85); border-radius: 4px;}");
            setWindowOpacity(0.50);
        }
    }
    QDialog::changeEvent(event);
}
