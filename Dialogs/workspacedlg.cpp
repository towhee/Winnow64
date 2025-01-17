#include "workspacedlg.h"
#include "ui_workspacedlg.h"
#include "Main/global.h"
#include <QDebug>
#include <QTimer>

WorkspaceDlg::WorkspaceDlg(QList<QString> *wsList, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Workspacedlg)
{
    ui->setupUi(this);
    this->mainWindow = parent;

    // get height for dropdown items from combobox height
    int h = ui->workspaceCB->height() - 8;
    // populate the dropdown list
    for (int i=0; i < wsList->count(); i++) {
        ui->workspaceCB->addItem(wsList->at(i));
        // set height for each row (setStylesheet approach did not work)
        ui->workspaceCB->model()->setData( ui->workspaceCB->model()->index(i, 0), QSize(h, h), Qt::SizeHintRole);
//        qDebug() << G::t.restart() << "\t" << "ui->workspaceCB i" << i
//                 << ui->workspaceCB->currentIndex();
    }
    editMode = true;
    #ifdef Q_OS_WIN
        Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif
}

WorkspaceDlg::~WorkspaceDlg()
{
    delete ui;
}

void WorkspaceDlg::on_deleteBtn_clicked()
{
    if (G::isLogger) G::log("WorkspaceDlg::on_deleteBtn_clicked");
    int n = ui->workspaceCB->currentIndex();
    emit deleteWorkspace(n);
    ui->workspaceCB->removeItem(n);
}

void WorkspaceDlg::on_reassignBtn_clicked()
{
    if (G::isLogger) G::log("WorkspaceDlg::on_reassignBtn_clicked");
    int n = ui->workspaceCB->currentIndex();
    emit reassignWorkspace(n);
    ui->status->setText(ui->workspaceCB->itemText(n) + " has been reassigned");
    QTimer::singleShot(2000, this, SLOT(clearStatus()));
}

void WorkspaceDlg::clearStatus()
{
    ui->status->setText("");
}

void WorkspaceDlg::on_workspaceCB_editTextChanged(const QString &name)
{
    if (G::isLogger) G::log("WorkspaceDlg::on_workspaceCB_editTextChanged");
    if (editMode) {
        bool isOkay = true;
        int n = ui->workspaceCB->currentIndex();
        ui->workspaceCB->setStyleSheet("QComboBox {color:rgb(229,229,229);}");
        // make sure another workspace name is not being duplicated
        for (int i=0; i<ui->workspaceCB->count(); i++) {
            // ignore current item
            if (i == n) continue;
            // different, try next item
            if (name != ui->workspaceCB->itemText(i)) continue;
            // oh, oh.  Duplicate of another workspace name
            isOkay = false;
            ui->workspaceCB->setStyleSheet("QComboBox {color:red;}");
        }
        if (isOkay) emit renameWorkspace(n, name);
        ui->workspaceCB->setItemText(ui->workspaceCB->currentIndex(), name);
    }
    editMode = true;
}

void WorkspaceDlg::on_workspaceCB_highlighted(int /*index*/)
{
    if (G::isLogger) G::log("WorkspaceDlg::on_workspaceCB_highlighted");
    editMode = false;
}

void WorkspaceDlg::report(QString signalName)
{
    int n = ui->workspaceCB->currentIndex();
    QString text = ui->workspaceCB->currentText();
    qDebug() << G::t.restart() << "\t" << signalName
             << "currentIndex =" << n
             << "editMode =" << editMode
             << "text =" << text;
}

void WorkspaceDlg::on_reportLinkButton_clicked()
{
//    int n = ui->workspaceCB->currentIndex();
//    emit reportWorkspaceNum(n);
}

void WorkspaceDlg::on_workspaceCB_currentIndexChanged(int /*index*/)
{
//    ui->workspaceIndexLbl->setText(QString::number(index));
}
