#ifndef PROCESSDLG_H
#define PROCESSDLG_H

#include <QDialog>
#include <QtWidgets>
#include "Main/global.h"
#include "Utilities/popup.h"

namespace Ui {

class Appdlg;
}

class Appdlg : public QDialog
{
    Q_OBJECT

public:
    Appdlg(QList<G::Pair> &externalApps, QWidget *parent = 0);
    ~Appdlg();

private slots:
    void on_addBtn_clicked();
    void on_removeBtn_clicked();
    void on_okBtn_clicked();
    void on_cancelBtn_clicked();
    void on_moveDown_clicked();
    void on_moveUp_clicked();

private:
    Ui::Appdlg *ui;

    G::Pair app;
    QList<G::Pair> &xApps;
    QString modifier;
    QVector<QString> shortcut = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};

    PopUp *popUp;
    void updateShortcuts();
    int getAppCount();
    void setFlags(int row);
};

#endif // PROCESSDLG_H
