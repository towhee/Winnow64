#include "zoomdlg.h"
#include "ui_zoomdlg.h"
#include "QDebug"

ZoomDlg::ZoomDlg(QWidget *parent, qreal zoom, QRect a, QRect c) : QDialog(parent),
    ui(new Ui::ZoomDlg)
{
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::Dialog | Qt::FramelessWindowHint);
    ui->setupUi(this);

    // update controls to current zoom factor
    zoomChange(zoom);

    // do some formatting (color: yellow; font-size: 18px;) NOTE keep in sync with winnow.css
    QString spinBoxStyle = "QSpinBox {color: yellow; font-size: 18px; background-color: rgb(111,111,111); border: 1px solid gray; selection-background-color: darkgray; border-radius: 5px; padding-left: 4px;}QSpinBox:hover, QSpinBox:focus {border-color: silver;}QSpinBox:disabled {color: rgb(77,77,77);background-color: rgb(88,88,88);}QSpinBox::up-button, QSpinBox::down-button  {width: 0px;border-width: 0px;}";
    ui->zoomSB->setStyleSheet(spinBoxStyle);

    // position in middle of main window and at the bottom of the central widget
    positionWindow(a, c);

    // remember previous zoom to do variance checks to prevent circular messaging
    oldZoom = zoom;
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
    int h = height();       // heiht of this dialog
    // anchor point = center of app and bottom central widget
    QPoint anchor = a.topLeft() + QPoint(0, c.height()) + QPoint(a.width() / 2, 0);
    setGeometry(anchor.x() - w/2, anchor.y() - h, w, h);
}

void ZoomDlg::zoomChange(qreal zoom)
{
/*
This slot receives signals from either imageView or compareView when the scale
changes.

Check for circular messaging where local change of scale was signalled
to ImageView and CompareView, where the scale is updated.  The scale change
triggers a signal back here, hence the variance check with the previous zoom.
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ZoomDlg::zoomChange" << zoom;
    #endif
    }
    zoom *= 100;
    qDebug() << "ImageView::zoomChange" << zoom;
    qreal variance = qFabs(1 - zoom / oldZoom);
    if (variance < .005) return;

    oldZoom = zoom;
    ui->zoomSB->setValue((int)zoom);
    if (zoom >= 1 && zoom <= 200) ui->zoomSlider->setValue(zoom);
}

void ZoomDlg::on_zoomSB_valueChanged(int value)
{
/*
When the zoom spinbox value changes we emit a signal to the slots in imageView
and compareView.

Check for circular messaging where local change of scale was signalled
to ZoomDlg, which converted to integer and signalled back here
*/
    {
    #ifdef ISDEBUG
    qDebug() << "ZoomDlg::on_zoomSB_valueChanged" << value;
    #endif
    }
    qDebug() << "ZoomDlg::on_zoomSB_valueChanged" << value;
    emit zoom((qreal)value/100);
}

void ZoomDlg::on_toggleZoomAmountBtn_clicked()
{
    qreal zoomVal = (qreal)ui->zoomSB->value() / 100;
    emit updateToggleZoom(zoomVal);
    QDialog::accept();
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

void ZoomDlg::on_radio150Button_clicked()
{
    emit zoom(1.5);
}

void ZoomDlg::on_radio200Button_clicked()
{
    emit zoom(2.0);
}
