#include "processdlg.h"
#include "ui_processdlg.h"

Processdlg::Processdlg(QMap<QString, QString>& externalApps, QWidget *parent)
    : xApps(externalApps), QDialog(parent), ui(new Ui::Processdlg)
{
    ui->setupUi(this);
    QStringList hdrs;
    hdrs << "Program display name" << "Program path";
    ui->apps->setHorizontalHeaderLabels(hdrs);
    ui->apps->horizontalHeader()->resizeSection(0, 200);
    ui->apps->horizontalHeader()->setStretchLastSection(true);

    QMapIterator<QString, QString> it(xApps);
    int rowCount = 0;
    while (it.hasNext()) {
        it.next();
        QTableWidgetItem *item0 = new QTableWidgetItem(it.key());
        QTableWidgetItem *item1 = new QTableWidgetItem(it.value());
        item1->setFlags(item1->flags() & ~Qt::ItemIsEditable); // non editable
        qDebug() << "Processdlg::Processdlg"
                 << "  it.key() =" << it.key()
                 << "  it.value() =" << it.value();
        ui->apps->insertRow(rowCount);
        ui->apps->setItem(rowCount, 0, item0);
        ui->apps->setItem(rowCount, 1, item1);
        rowCount++;
    }
}

Processdlg::~Processdlg()
{
    delete ui;
}

void Processdlg::reject()
{
    qDebug() << "Leaving...";
    xApps.clear();
    for (int i = 0; i < ui->apps->rowCount(); ++i) {
        QString key = ui->apps->model()->index(i,0).data().toString();
        QString val = ui->apps->model()->index(i,1).data().toString();
        xApps[key] = val;
        qDebug() << xApps;
    }
    QDialog::reject();
}

void Processdlg::on_addBtn_clicked()
{
    int rowCount = ui->apps->rowCount();
    if (rowCount >= 10) {
        qDebug() << "rowCount" << rowCount;
        return;
    }
    QString appPath = QFileDialog::getOpenFileName(this,
         tr("Select program executable"), "/home");
    QFileInfo fileInfo(appPath);
    QString appName = fileInfo.baseName();
    appName[0].toUpper();
    qDebug() << appName;
    QTableWidgetItem *item0 = new QTableWidgetItem(appName);
    QTableWidgetItem *item1 = new QTableWidgetItem(fileInfo.absoluteFilePath());
    ui->apps->insertRow(rowCount);
    ui->apps->setItem(rowCount, 0, item0);
    ui->apps->setItem(rowCount, 1, item1);
//    delete item0;
//    delete item1;
}

void Processdlg::on_removeBtn_clicked()
{
    QModelIndex idx;
    qDebug() << "on_removeBtn_clicked   current row =" << ui->apps->currentRow();
    ui->apps->removeRow(ui->apps->currentRow());
}
