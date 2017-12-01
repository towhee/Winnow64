#include "zoomdlg.h"
#include "ui_zoomdlg.h"
#include "QDebug"

ZoomDlg::ZoomDlg(QWidget *parent, int zoom, QRect a, QRect c) : QDialog(parent),
    ui(new Ui::ZoomDlg)
{
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::Dialog | Qt::FramelessWindowHint);
    ui->setupUi(this);
    zoomChange(zoom);

    QString spinBoxStyle = "QSpinBox {color: yellow; font-size: 18px; background-color: rgb(111,111,111); border: 1px solid gray; selection-background-color: darkgray; border-radius: 5px; padding-left: 4px;}QSpinBox:hover, QSpinBox:focus {border-color: silver;}QSpinBox:disabled {color: rgb(77,77,77);background-color: rgb(88,88,88);}QSpinBox::up-button, QSpinBox::down-button  {width: 0px;border-width: 0px;}";
    ui->zoomSB->setStyleSheet(spinBoxStyle);
    positionWindow(a, c);
}

ZoomDlg::~ZoomDlg()
{
    delete ui;
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

void ZoomDlg::zoomChange(int zoom)
{
    ui->zoomSB->setValue(zoom);
    if (zoom >= 1 && zoom <= 200) ui->zoomSlider->setValue(zoom);
}

void ZoomDlg::on_zoomSB_valueChanged(int value)
{
    qreal zoomVal = (qreal)value / 100;
    emit zoom(zoomVal);
}

void ZoomDlg::on_toggleZoomAmountBtn_clicked()
{
    qreal zoomVal = (qreal)ui->zoomSB->value() / 100;
    emit updateToggleZoom(zoomVal);
    QDialog::accept();
}

void ZoomDlg::on_radio25Button_clicked()
{

}

void ZoomDlg::on_radio50Button_clicked()
{

}

void ZoomDlg::on_radio66Button_clicked()
{

}

void ZoomDlg::on_radio100Button_clicked()
{

}

void ZoomDlg::on_radio150Button_clicked()
{

}

void ZoomDlg::on_radio200Button_clicked()
{

}
