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
    Appdlg(QList<G::App> &externalApps, QWidget *parent = nullptr);
    ~Appdlg() override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void on_addBtn_clicked();
    void on_removeBtn_clicked();
    void on_changePathBtn_clicked();
    void on_okBtn_clicked();
//    void on_cancelBtn_clicked();
    void on_moveDown_clicked();
    void on_moveUp_clicked();
//    void currentChanged(const QModelIndex &current, const QModelIndex &previous);
    void checkProgramsExist(int currentRow = 0, int currentColumn = 0, int previousRow = 0, int previousColumn = 0);

private:
    Ui::Appdlg *ui;

    G::App app;
    QList<G::App> &xApps;
    QString modifier;
    QVector<QString> shortcut = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};

    void setScreenDependencies();
    void updateShortcuts();
    int getAppCount();
    void setFlags(int row);
};

#endif // PROCESSDLG_H
