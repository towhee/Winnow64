#include "imagedlg.h"
#include "ui_imagedlg.h"

ImageDlg::ImageDlg(const QImage &image, QSize size, QString title, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ImageDlg)
{
    ui->setupUi(this);
    resize(size);
//    QPixmap pm = QPixmap::fromImage(image.scaled(size, Qt::KeepAspectRatio));
    ui->label->setPixmap(QPixmap::fromImage(image.scaled(size, Qt::KeepAspectRatio)));
    setWindowTitle(title);
    #ifdef Q_OS_WIN
        Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif
}

ImageDlg::~ImageDlg()
{
    delete ui;
}
