#include "zoomdlg.h"
#include "ui_zoomdlg.h"
#include "QDebug"

ZoomDlg::ZoomDlg(QWidget *parent, int zoom) :
    QDialog(parent),
    ui(new Ui::ZoomDlg)
{
    ui->setupUi(this);
    qDebug() << "zoom" << zoom;
    ui->zoomSB->setValue(zoom);
    if (zoom >= 1 && zoom <= 400) ui->zoomSlider->setValue(zoom);
}

ZoomDlg::~ZoomDlg()
{
    delete ui;
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
