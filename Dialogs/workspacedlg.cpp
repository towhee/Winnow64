#include "workspacedlg.h"
#include "ui_workspacedlg.h"
#include "global.h"
#include <QDebug>
#include <QTimer>

WorkspaceDlg::WorkspaceDlg(QList<QString> *wsList, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Workspacedlg)
{
    ui->setupUi(this);
    this->mainWindow = parent;

    for (int i=0; i < wsList->count(); i++) {
        ui->workspaceCB->addItem(wsList->at(i));
        qDebug() << "ui->workspaceCB i" << i
                 << ui->workspaceCB->currentIndex();
    }
    editMode = true;
}

WorkspaceDlg::~WorkspaceDlg()
{
    delete ui;
}

void WorkspaceDlg::on_deleteBtn_clicked()
{
    {
    #ifdef ISDEBUG
    qDebug() << "WorkspaceDlg::on_deleteBtn_clicked";
    #endif
    }
    int n = ui->workspaceCB->currentIndex();
//    QString name = ui->workspaceCB->itemText(n);
    emit deleteWorkspace(n);
    ui->workspaceCB->removeItem(n);
}

void WorkspaceDlg::on_reassignBtn_clicked()
{
    {
    #ifdef ISDEBUG
    qDebug() << "WorkspaceDlg::on_reassignBtn_clicked";
    #endif
    }
    int n = ui->workspaceCB->currentIndex();
    emit reassignWorkspace(n);
    ui->status->setText(ui->workspaceCB->itemText(n) + " has been reassigned");
    QTimer::singleShot(500, this, SLOT(clearStatus()));
}

void WorkspaceDlg::clearStatus()
{
    ui->status->setText("");
}

void WorkspaceDlg::on_workspaceCB_editTextChanged(const QString &name)
{
    {
    #ifdef ISDEBUG
    qDebug() << "WorkspaceDlg::on_workspaceCB_editTextChanged";
    #endif
    }
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

void WorkspaceDlg::on_workspaceCB_highlighted(int index)
{
    {
    #ifdef ISDEBUG
    qDebug() << "WorkspaceDlg::on_workspaceCB_highlighted";
    #endif
    }
    editMode = false;
}

void WorkspaceDlg::report(QString signalName)
{
    int n = ui->workspaceCB->currentIndex();
    QString text = ui->workspaceCB->currentText();
    qDebug() << signalName
             << "currentIndex =" << n
             << "editMode =" << editMode
             << "text =" << text;
}
