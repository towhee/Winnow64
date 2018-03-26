#ifndef PROCESSDLG_H
#define PROCESSDLG_H

#include <QDialog>
#include <QtWidgets>
#include "Main/global.h"

namespace Ui {
class Appdlg;
}

class Appdlg : public QDialog
{
    Q_OBJECT

public:
    Appdlg(QMap<QString, QString>& externalApps, QWidget *parent = 0);
    ~Appdlg();

private slots:
    void on_addBtn_clicked();
    void on_removeBtn_clicked();

    void on_okBtn_clicked();

private:
    Ui::Appdlg *ui;
    QMap<QString, QString>& xApps;
    void reject();
};

#endif // PROCESSDLG_H
