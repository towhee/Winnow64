#include "appdlg.h"
#include "ui_appdlg.h"
#include <QDebug>

/*
This class maintains a list of external programs that Winnow can use to open with the
current selection to files. The external apps list is passed as the QList externalApps,
and is edited internally as xApps. The list elements are pairs of QString (name/path) for
each app. In order to pass this as an argument from MW to here it is necessary to put the
definition for the struct "Pair" in global.h.

The external app list is added to the table ui->appsTable for display in the dialog. The
user can add apps, delete apps, change the order and edit the app display name.

When the changes are saved xApps is cleared and then rebuilt by iterating the
ui->appsTable table.

Cancel exits with no changes to the external app list.
*/

Appdlg::Appdlg(QList<G::App> &externalApps, QWidget *parent)
    : QDialog(parent), ui(new Ui::Appdlg), xApps(externalApps)
{
    ui->setupUi(this);

    #ifdef Q_OS_LINIX
    //
    #endif
    #ifdef Q_OS_WIN
        modifier = "Alt + ";
    #endif
    #ifdef Q_OS_MAC
        modifier = "Opt + ";
    #endif

    /* set widths and heights that are dependent on the display screen settings in case
    the user drags the dialog to another screen/monitor. */

    QStringList hdrs;
    hdrs << "Shortcut" << "Display name" << "Program path" << "Program arguments";
    ui->appsTable->setHorizontalHeaderLabels(hdrs);
    ui->appsTable->horizontalHeader()->setStretchLastSection(true);
    ui->appsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->appsTable->verticalHeader()->setSectionsClickable(false);
    setScreenDependencies();

    setStyleSheet(
        "QLineEdit {"
            "selection-background-color: gray;"
            "border: none;"
        "}"
        "QLineEdit:focus {"
            "background-color:" + G::selectionColor.name() + ";"
        "}"
        ";"
    );

    /* fill the table with the shortcuts and blanks for the name and path.
    All table editing after this simply renames the name and path and no rows
    are inserted or deleted.  Editing is turned of for all the items, as we
    do not want the user trying to pick or edit blank rows. */
    for(int row = 0; row < 10; ++ row) {
        QTableWidgetItem *item0 = new QTableWidgetItem(modifier + shortcut[row]);
        QTableWidgetItem *item1 = new QTableWidgetItem("");
        QTableWidgetItem *item2 = new QTableWidgetItem("");
        QTableWidgetItem *item3 = new QTableWidgetItem("");

        // center the shortcuts
        item0->setData(Qt::TextAlignmentRole,Qt::AlignCenter);

        ui->appsTable->insertRow(row);
        ui->appsTable->setItem(row, 0, item0);
        ui->appsTable->setItem(row, 1, item1);
        ui->appsTable->setItem(row, 2, item2);
        ui->appsTable->setItem(row, 3, item3);

        setFlags(row);
    }

    // second pass to enter the existing external apps
    int rows = xApps.length();
    for (int row = 0; row < rows; ++row) {
        QString name = xApps.at(row).name;
        QString path = xApps.at(row).path;
        QString args = xApps.at(row).args;
        QFileInfo info(path);
        if (name == "") name = info.baseName();
        ui->appsTable->item(row, 1)->setText(name);
        ui->appsTable->item(row, 2)->setText(path);
        ui->appsTable->item(row, 3)->setText(args);
        setFlags(row);
    }
    checkProgramsExist();

    if (rows > 0) ui->appsTable->selectRow(0);

    connect(ui->appsTable, &QTableWidget::currentCellChanged, this, &Appdlg::checkProgramsExist);
}

Appdlg::~Appdlg()
{
    delete ui;
}

void Appdlg::setFlags(int row)
{
    QTableWidgetItem *item0 = ui->appsTable->item(row, 0);
    QTableWidgetItem *item1 = ui->appsTable->item(row, 1);
    QTableWidgetItem *item2 = ui->appsTable->item(row, 2);
    QTableWidgetItem *item3 = ui->appsTable->item(row, 3);

    if (ui->appsTable->item(row, 1)->text() == "") {
        item0->setFlags(Qt::NoItemFlags); // disabled
        item1->setFlags(Qt::NoItemFlags); // disabled
        item2->setFlags(Qt::NoItemFlags); // disabled
        item3->setFlags(Qt::NoItemFlags); // disabled
    }
    else {
        item0->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled); // non editable
        item1->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable); // editable
        item2->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable); // editable
        item3->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable); // editable
    }
}

void Appdlg::updateShortcuts()
{
    for (int row = 0; row < 10; ++row) {
        QTableWidgetItem *item0 = new QTableWidgetItem(modifier + shortcut[row]);
        ui->appsTable->setItem(row, 0, item0);
    }
}

int Appdlg::getAppCount()
{
    int row;
    for(row = 0; row < 10; ++row) {
        if (ui->appsTable->item(row, 1)->text() == "") break;
    }
    return row;
}

void Appdlg::on_addBtn_clicked()
{
    int row = getAppCount();
    if (row > 9) {
        return;
    }
    QString appPath = QFileDialog::getOpenFileName(this,
         tr("Select program executable"), "/home");
    QFileInfo fileInfo(appPath);
    QString appName = fileInfo.baseName();
    appName[0].toUpper();
    ui->appsTable->item(row, 1)->setText(appName);
    ui->appsTable->item(row, 2)->setText(fileInfo.absoluteFilePath());
    setFlags(row);
    ui->appsTable->selectRow(row);
}

void Appdlg::on_removeBtn_clicked()
{
    int rowToRemove = ui->appsTable->currentRow();
    if (rowToRemove < 0 || ui->appsTable->currentRow() > 9) {
        G::popUp->showPopup("No row selected");
        return;
    }

    if(ui->appsTable->item(rowToRemove, 1)->text() == "") return;

    QString name;
    QString path;
    QString args;
    int rows = getAppCount() - 1;
    for (int row = 0; row < rows; ++row) {
        int n = 0;
        if(row >= rowToRemove) n = 1;
        name = ui->appsTable->item(row + n, 1)->text();
        path = ui->appsTable->item(row + n, 2)->text();
        args = ui->appsTable->item(row + n, 3)->text();
        ui->appsTable->item(row, 1)->setText(name);
        ui->appsTable->item(row, 2)->setText(path);
        ui->appsTable->item(row, 3)->setText(args);
    }
    ui->appsTable->item(rows, 1)->setText("");
    ui->appsTable->item(rows, 2)->setText("");
    ui->appsTable->item(rows, 3)->setText("");

    setFlags(rows);
    int row = rowToRemove;
    if (row >= rows - 1) --row;
    if (row < 0) row = 0;
    ui->appsTable->selectRow(row);
}

void Appdlg::checkProgramsExist(int /*currentRow*/, int /*currentColumn*/, int /*previousRow*/, int /*previousColumn*/)
{
    QColor defaultColor = QColor(G::textShade, G::textShade, G::textShade);
    QColor warningColor = QColor(128,0,0);
    int rows = static_cast<int>(xApps.length());
        for (int row = 0; row < rows; ++row) {
        QFile test(ui->appsTable->item(row, 2)->text());
        if (test.exists()) {
            ui->appsTable->item(row, 2)->setForeground(defaultColor);
        }
        else {
            ui->appsTable->item(row, 2)->setForeground(warningColor);
        }
    }
}

void Appdlg::on_changePathBtn_clicked()
{
    int row = ui->appsTable->currentRow();
    QString appPath = QFileDialog::getOpenFileName(this,
         tr("Select program executable"), "/home");
    QFileInfo fileInfo(appPath);
    ui->appsTable->item(row, 2)->setText(fileInfo.absoluteFilePath());
    checkProgramsExist();
}

void Appdlg::on_okBtn_clicked()
{
    int rows = getAppCount();
    xApps.clear();
    for (int row = 0; row < rows; ++row) {
        app.name = ui->appsTable->model()->index(row,1).data().toString();
        app.path = ui->appsTable->model()->index(row,2).data().toString();
        app.args = ui->appsTable->model()->index(row,3).data().toString();
        xApps.append(app);
    }
    accept();
}

//void Appdlg::on_cancelBtn_clicked()
//{
//    reject();
//}

void Appdlg::on_moveDown_clicked()
{
    int row = ui->appsTable->currentRow();
    if(row > 8) return;
    if(ui->appsTable->item(row, 1)->text() == "") return;
    if(ui->appsTable->item(row + 1, 1)->text() == "") return;
    // count rows with app name - xApps not updated until save
    int rows = xApps.length();
    if(row > rows - 2) return;

    QTableWidgetItem *srcName = ui->appsTable->takeItem(row, 1);
    QTableWidgetItem *srcPath = ui->appsTable->takeItem(row, 2);
    QTableWidgetItem *srcArgs = ui->appsTable->takeItem(row, 3);
    QTableWidgetItem *dstName = ui->appsTable->takeItem(row + 1, 1);
    QTableWidgetItem *dstPath = ui->appsTable->takeItem(row + 1, 2);
    QTableWidgetItem *dstArgs = ui->appsTable->takeItem(row + 1, 3);

    ui->appsTable->setItem(row, 1, dstName);
    ui->appsTable->setItem(row, 2, dstPath);
    ui->appsTable->setItem(row, 3, dstArgs);
    ui->appsTable->setItem(row + 1, 1, srcName);
    ui->appsTable->setItem(row + 1, 2, srcPath);
    ui->appsTable->setItem(row + 1, 3, srcArgs);

    ui->appsTable->selectRow(row + 1);
}

void Appdlg::on_moveUp_clicked()
{
    int row = ui->appsTable->currentRow();
    if(row < 1) return;
    if(ui->appsTable->item(row, 1)->text() == "") return;
    if(ui->appsTable->item(row - 1, 1)->text() == "") return;
    // count rows with app name - xApps not updated until save
    if(row < 1) return;

    QTableWidgetItem *srcName = ui->appsTable->takeItem(row, 1);
    QTableWidgetItem *srcPath = ui->appsTable->takeItem(row, 2);
    QTableWidgetItem *srcArgs = ui->appsTable->takeItem(row, 3);
    QTableWidgetItem *dstName = ui->appsTable->takeItem(row - 1, 1);
    QTableWidgetItem *dstPath = ui->appsTable->takeItem(row - 1, 2);
    QTableWidgetItem *dstArgs = ui->appsTable->takeItem(row - 1, 3);

    ui->appsTable->setItem(row, 1, dstName);
    ui->appsTable->setItem(row, 2, dstPath);
    ui->appsTable->setItem(row, 3, dstArgs);
    ui->appsTable->setItem(row - 1, 1, srcName);
    ui->appsTable->setItem(row - 1, 2, srcPath);
    ui->appsTable->setItem(row - 1, 3, srcArgs);

    ui->appsTable->selectRow(row - 1);
}

void Appdlg::paintEvent(QPaintEvent *event)
{
//    setScreenDependencies();
    QDialog::paintEvent(event);
}

void Appdlg::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    setScreenDependencies();
}

void Appdlg::setScreenDependencies()
{
    QFontMetrics fm(this->font());
    int rowHeight = static_cast<int>(fm.boundingRect("X").height() * 1.5);
    ui->appsTable->horizontalHeader()->setFixedHeight(rowHeight);
    ui->appsTable->verticalHeader()->setDefaultSectionSize(rowHeight);

    int hdr0Width = fm.boundingRect("==Shortcut==").width();
    int hdr1Width = fm.boundingRect("==Adobe Photoshop 2022==").width();
    int hdr2Width = fm.boundingRect("/Applications/Adobe Photoshop 2022/Adobe Photoshop 2022.app=========").width();
//    int hdr3Width = fm.boundingRect("------Display args------").width();
//    int hdr2Width = ui->appsTable->viewport()->width() - hdr0Width - hdr1Width - hdr3Width;
    ui->appsTable->setColumnWidth(0, hdr0Width);
    ui->appsTable->setColumnWidth(1, hdr1Width);
    ui->appsTable->setColumnWidth(2, hdr2Width);
//    ui->appsTable->setColumnWidth(3, hdr3Width);
}
