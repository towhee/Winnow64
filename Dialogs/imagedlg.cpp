#include "imagedlg.h"
#include "ui_imagedlg.h"

ImageDlg::ImageDlg(const QImage &image, QString title, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ImageDlg)
{
    ui->setupUi(this);
    QSize size(800,800);
    resize(size);
//    QPixmap pm = QPixmap::fromImage(image.scaled(size, Qt::KeepAspectRatio));
    ui->label->setPixmap(QPixmap::fromImage(image.scaled(size, Qt::KeepAspectRatio)));
    setWindowTitle(title);
}

ImageDlg::~ImageDlg()
{
    delete ui;
}
