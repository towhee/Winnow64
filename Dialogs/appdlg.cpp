#include "appdlg.h"
#include "ui_appdlg.h"
#include <QDebug>

/*
This class maintains a list of external programs that Winnow can use to open
with the current selection to files.  The external apps list is passed as
the QMap externalApps, and is edited internally as xApps.

The external app list is added to the table ui->appsTable for display in the
dialog.  The user can add apps, delete apps and edit the app display name.

When the changes are saved xApps is cleared and then rebuilt by iterating the
ui->appsTable table.
*/

Appdlg::Appdlg(QList<G::Pair> &externalApps, QWidget *parent)
    : QDialog(parent), ui(new Ui::Appdlg), xApps(externalApps)
{
    ui->setupUi(this);

    {
#ifdef Q_OS_LINIX
//
#endif
#ifdef Q_OS_WIN
    modifier = "Alt + ";
#endif
#ifdef Q_OS_MAC
    modifier = "Opt + ";;
#endif

    }
    QStringList hdrs;
    hdrs << "Shortcut" << "Program display name" << "Program path";
    ui->appsTable->setHorizontalHeaderLabels(hdrs);
    ui->appsTable->horizontalHeader()->resizeSection(0, 100);
    ui->appsTable->horizontalHeader()->resizeSection(1, 200);
    ui->appsTable->horizontalHeader()->setStretchLastSection(true);
    ui->appsTable->horizontalHeader()->setFixedHeight(24);
    ui->appsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->appsTable->verticalHeader()->setSectionsClickable(false);

    for(int row = 0; row < 10; ++ row) {
        QTableWidgetItem *item0 = new QTableWidgetItem(modifier + shortcut[row]);
        QTableWidgetItem *item1 = new QTableWidgetItem("");
        QTableWidgetItem *item2 = new QTableWidgetItem("");

        item0->setData(Qt::TextAlignmentRole,Qt::AlignCenter);

        item0->setFlags(item0->flags() & ~Qt::ItemIsEnabled); // disabled
        item0->setFlags(item0->flags() & ~Qt::ItemIsEditable); // non editable

        item1->setFlags(item1->flags() & ~Qt::ItemIsEnabled); // disabled
        item1->setFlags(item1->flags() & ~Qt::ItemIsEditable); // non editable

        item2->setFlags(item2->flags() & ~Qt::ItemIsEnabled); // disabled
        item2->setFlags(item2->flags() & ~Qt::ItemIsEditable); // non editable

        ui->appsTable->insertRow(row);
        ui->appsTable->setItem(row, 0, item0);
        ui->appsTable->setItem(row, 1, item1);
        ui->appsTable->setItem(row, 2, item2);
    }

    int rows = xApps.length();
    qDebug() << Qt::ItemIsEnabled << ~Qt::ItemIsEnabled;
    for(int row = 0; row < rows; ++row) {
        QTableWidgetItem *item0 = new QTableWidgetItem(modifier + shortcut[row]);
        QTableWidgetItem *item1 = new QTableWidgetItem(xApps.at(row).name);
        QTableWidgetItem *item2 = new QTableWidgetItem(xApps.at(row).path);
        item0->setData(Qt::TextAlignmentRole,Qt::AlignCenter);

//        item0->setFlags(item0->flags() & Qt::ItemIsEnabled); // disabled
        item0->setFlags(item0->flags() & ~Qt::ItemIsEditable); // non editable

//        item1->setFlags(item1->flags() & Qt::ItemIsEnabled); // enabled
//        item1->setFlags(item1->flags() & Qt::ItemIsEditable); // editable

//        item2->setFlags(item2->flags() & Qt::ItemIsEnabled); // enabled
        item2->setFlags(item2->flags() & ~Qt::ItemIsEditable); // non editable

        ui->appsTable->setItem(row, 0, item0);
        ui->appsTable->setItem(row, 1, item1);
        ui->appsTable->setItem(row, 2, item2);
    }
}

Appdlg::~Appdlg()
{
    delete ui;
}

void Appdlg::on_addBtn_clicked()
{
    int rowCount = ui->appsTable->rowCount();
    if (rowCount > 10) {
        return;
    }
    QString appPath = QFileDialog::getOpenFileName(this,
         tr("Select program executable"), "/home");
    QFileInfo fileInfo(appPath);
    QString appName = fileInfo.baseName();
    appName[0].toUpper();
    QTableWidgetItem *item = new QTableWidgetItem(modifier + shortcut[rowCount]);
    QTableWidgetItem *item0 = new QTableWidgetItem(appName);
    QTableWidgetItem *item1 = new QTableWidgetItem(fileInfo.absoluteFilePath());
    ui->appsTable->insertRow(rowCount);
    ui->appsTable->setItem(rowCount, 0, item);
    ui->appsTable->setItem(rowCount, 1, item0);
    ui->appsTable->setItem(rowCount, 2, item1);
}

void Appdlg::on_removeBtn_clicked()
{
    ui->appsTable->removeRow(ui->appsTable->currentRow());
}

void Appdlg::on_okBtn_clicked()
{
    int rows = ui->appsTable->rowCount();
    xApps.clear();
    for (int row = 0; row < rows; ++row) {
        app.name = ui->appsTable->model()->index(row,1).data().toString();
        app.path = ui->appsTable->model()->index(row,2).data().toString();
        qDebug() << "on_okBtn_clicked" << app.name << app.path;
        xApps.append(app);
    }
    accept();
}

void Appdlg::on_cancelBtn_clicked()
{
    reject();
}
