#include "tokendlg.h"
#include "ui_tokendlg.h"
#include <QDebug>

TokenDlg::TokenDlg(QStringList &tokenList, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TokenDlg)
{
    ui->setupUi(this);
    for (const auto& i : tokenList) {
        qDebug() << i;
    }
    ui->tokenList->insertItems(0, tokenList);
}

TokenDlg::~TokenDlg()
{
    delete ui;
}
