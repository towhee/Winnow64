#include "appdlg.h"
#include "ui_appdlg.h"

/*
This class maintains a list of external programs that Winnow can use to open
with the current selection to files.  The external apps list is passed as
the QMap externalApps, and is edited internally as xApps.

The external app list is added to the table ui->appsTable for display in the
dialog.  The user can add apps, delete apps and edit the app display name.

When the changes are saved xApps is cleared and then rebuilt by iterating the
ui->appsTable table.
*/

Appdlg::Appdlg(QMap<QString, QString>& externalApps, QWidget *parent)
    : QDialog(parent), ui(new Ui::Appdlg), xApps(externalApps)
{
    ui->setupUi(this);
    QStringList hdrs;
    hdrs << "Program display name" << "Program path";
    ui->appsTable->setHorizontalHeaderLabels(hdrs);
    ui->appsTable->horizontalHeader()->resizeSection(0, 200);
    ui->appsTable->horizontalHeader()->setStretchLastSection(true);

    QMapIterator<QString, QString> it(xApps);
    int rowCount = 0;
    while (it.hasNext()) {
        it.next();
        QTableWidgetItem *item0 = new QTableWidgetItem(it.key());
        QTableWidgetItem *item1 = new QTableWidgetItem(it.value());
        item1->setFlags(item1->flags() & ~Qt::ItemIsEditable); // non editable
        ui->appsTable->insertRow(rowCount);
        ui->appsTable->setItem(rowCount, 0, item0);
        ui->appsTable->setItem(rowCount, 1, item1);
        rowCount++;
    }
}

Appdlg::~Appdlg()
{
    delete ui;
}

void Appdlg::on_addBtn_clicked()
{
    int rowCount = ui->appsTable->rowCount();
    if (rowCount >= 10) {
//        qDebug() << "rowCount" << rowCount;
        return;
    }
    QString appPath = QFileDialog::getOpenFileName(this,
         tr("Select program executable"), "/home");
    QFileInfo fileInfo(appPath);
    QString appName = fileInfo.baseName();
    appName[0].toUpper();
//    qDebug() << G::t.restart() << "\t" << appName;
    QTableWidgetItem *item0 = new QTableWidgetItem(appName);
    QTableWidgetItem *item1 = new QTableWidgetItem(fileInfo.absoluteFilePath());
    ui->appsTable->insertRow(rowCount);
    ui->appsTable->setItem(rowCount, 0, item0);
    ui->appsTable->setItem(rowCount, 1, item1);
}

void Appdlg::on_removeBtn_clicked()
{
    QModelIndex idx;
    qDebug() << G::t.restart() << "\t" << "on_removeBtn_clicked   current row =" << ui->appsTable->currentRow();
    ui->appsTable->removeRow(ui->appsTable->currentRow());
}

void Appdlg::on_okBtn_clicked()
{
    int rows = ui->appsTable->rowCount();
    xApps.clear();
    qDebug() << "Accepted... rowCount =" << rows;
    for (int row = 0; row < rows; ++row) {
        QString key = ui->appsTable->model()->index(row,0).data().toString();
        QString val = ui->appsTable->model()->index(row,1).data().toString();
        qDebug() << key << val;
        xApps[key] = val;
    }
    qDebug() << "xApps =" << xApps;
    accept();
}

void Appdlg::on_cancelBtn_clicked()
{
    reject();
}
