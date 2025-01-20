#include "selectionorpicksdlg.h"
#include "ui_selectionorpicksdlg.h"
#include "Main/global.h"

SelectionOrPicksDlg::SelectionOrPicksDlg(Option &option, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SelectionOrPicksDlg),
    option(option)
{
    ui->setupUi(this);
    setStyleSheet(G::css);
    #ifdef Q_OS_WIN
        Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif
}

SelectionOrPicksDlg::~SelectionOrPicksDlg()
{
    delete ui;
}

void SelectionOrPicksDlg::on_cancelBtn_clicked()
{
    option = Cancel;
    reject();
}

void SelectionOrPicksDlg::on_usePicksBtn_clicked()
{
    option = Picks;
    accept();
}
void SelectionOrPicksDlg::on_useSelectionBtn_clicked()
{
    option = Selection;
    accept();
}
