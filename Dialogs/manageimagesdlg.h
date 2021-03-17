#ifndef MANAGEIMAGESDLG_H
#define MANAGEIMAGESDLG_H

#include <QtWidgets>
#include "Dialogs/patterndlg.h"
#include "Main/global.h"

namespace Ui {
class ManageImagesDlg;
}

class ManageImagesDlg : public QDialog
{
    Q_OBJECT

public:
    explicit ManageImagesDlg(QString title, QSettings *setting, QString settingPath,
                             QWidget *parent = nullptr);
    ~ManageImagesDlg();

private slots:
    void on_newBtn_clicked();

    void on_deleteBtn_clicked();

    void on_doneBtn_clicked();

private:
    Ui::ManageImagesDlg *ui;
    QSettings *setting;
    QString settingPath;
    void extractTile(QPixmap &src);
    int ht;
};

#endif // MANAGEIMAGESDLG_H
