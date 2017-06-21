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
    }
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
    QString name = ui->workspaceCB->itemText(n);
    qDebug() << "WorkspaceDlg::on_deleteBtn_clicked  Item count" << ui->workspaceCB->count()
             << "Current index" << n;;
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

void WorkspaceDlg::on_workspaceCB_activated(int idx)
{
    {
    #ifdef ISDEBUG
    qDebug() << "WorkspaceDlg::on_workspaceCB_activated";
    #endif
    }
    qDebug() << "on_workspaceCB_activated" << idx;
//    currentName = name;
    activatedIdx = idx;
}

void WorkspaceDlg::on_workspaceCB_editTextChanged(const QString &name)
{
    {
    #ifdef ISDEBUG
    qDebug() << "WorkspaceDlg::workspaceCB_editTextChanged";
    #endif
    }
    bool isOkay = true;
    // check for re-entry when editing text in combo
    static int x = 0;
    int n = ui->workspaceCB->currentIndex();
    qDebug() << "WorkspaceDlg::workspaceCB_editTextChanged"
             << "name =" << name << "currentName =" << currentName
             << "x =" << x;
    // n = current item, activatedIdx = prev item (activated event is called
    // after editTextChanged event)
    if (!(n == activatedIdx)) {
        // New item triggered editTextChanged, ignore
        x = 0;
        return;
    }
    // edit text is happening
    ui->workspaceCB->setStyleSheet("QComboBox {color:rgb(229,229,229);}");
    if (x > 0) {
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
    x++;
}

