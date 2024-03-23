#include "helppixeldelta.h"
#include "ui_helppixeldelta.h"

HelpPixelDelta::HelpPixelDelta(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::HelpPixelDelta)
{
    ui->setupUi(this);
}

HelpPixelDelta::~HelpPixelDelta()
{
    delete ui;
}
