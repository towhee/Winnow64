#ifndef EDITLISTDLG_H
#define EDITLISTDLG_H

#include <QtWidgets>
#include "Main/global.h"
#include "Utilities/win.h"

namespace Ui {
    class EditListDlg;
}

class EditListDlg : public QDialog
{
    Q_OBJECT

public:
    explicit EditListDlg(QStringListModel *model, QString title, QWidget *parent = nullptr);
    ~EditListDlg() override;

private slots:
//    void itemDoubleClicked(QTableWidgetItem *item);
//    void itemChanged(QTableWidgetItem *item);
//    void on_newBtn_clicked();
    void on_deleteBtn_clicked();
    void on_doneBtn_clicked();

private:
    Ui::EditListDlg *ui;
    QStringListModel *model;
    int getRow(QString s);
};

#endif // EDITLISTDLG_H
