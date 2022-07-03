#include "editlistdlg.h"
#include "ui_editlistdlg.h"

EditListDlg::EditListDlg(QStringListModel *model, QString title, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EditListDlg)
{
    qDebug() << CLASSFUNCTION << title;
    this->model = model;
    setStyleSheet(G::css);
    ui->setupUi(this);
    setWindowTitle(title);
    ui->listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->listWidget->addItems(model->stringList());
}

EditListDlg::~EditListDlg()
{
    delete ui;
}

void EditListDlg::on_deleteBtn_clicked()
{
    QList<QListWidgetItem *> toDeleteList;
    toDeleteList << ui->listWidget->selectedItems();

    for (int i = 0; i < toDeleteList.length(); ++i) {
        QString string = toDeleteList.at(i)->text();
        QString sKey = "IngestDescriptionCompleter/" + string;
        int modelRow =  getRow(string);
        int listWidgetRow = ui->listWidget->row(toDeleteList.at(i));
        // remove from settings
        G::settings->remove(sKey);
        // remove from dialog list
        ui->listWidget->model()->removeRow(listWidgetRow);
        // remove from model used by calling widget ie a completer in ingest
        model->removeRow(modelRow);
        // scroll to next row
        int listWidgetRows = ui->listWidget->model()->rowCount();
        if (listWidgetRow >= listWidgetRows) listWidgetRow = listWidgetRows - 1;
        ui->listWidget->scrollTo(ui->listWidget->model()->index(listWidgetRow,0));
    }
}

int EditListDlg::getRow(QString s)
{
    for (int row = 0; row < model->rowCount(); ++row) {
        if (model->index(row, 0).data().toString() == s) return row;
    }
    return -1;
}

void EditListDlg::on_doneBtn_clicked()
{
    accept();
}
