#include "tableview.h"
#include "ui_tableview.h"

TableView::TableView(ThumbView *thumbView, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TableView)
{
    ui->setupUi(this);
    ui->tableView->setModel(thumbView->thumbViewFilter);
//    ui->tableView->setModel(thumbView->thumbViewModel);
    ui->tableView->setSortingEnabled(true);
    ui->tableView->setAlternatingRowColors(true);
    ui->tableView->resizeColumnsToContents();
}

TableView::~TableView()
{
    delete ui;
}