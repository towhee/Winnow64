#include "imagedlg.h"
#include "ui_imagedlg.h"

ImageDlg::ImageDlg(QPixmap &pm, QString title, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ImageDlg)
{
    ui->setupUi(this);
    ui->label->setPixmap(pm);
    setWindowTitle(title);
}

ImageDlg::~ImageDlg()
{
    delete ui;
}
