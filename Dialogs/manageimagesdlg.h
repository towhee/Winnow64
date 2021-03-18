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
    ~ManageImagesDlg() override;

public slots:
    void save(QPixmap *src);

private slots:
    void on_newBtn_clicked();
    void on_deleteBtn_clicked();
    void on_doneBtn_clicked();

private:
    void extractTile(QPixmap &src);
    Ui::ManageImagesDlg *ui;
    QSettings *setting;
    QString settingPath;
    QString fPath;
    int ht;
};

#endif // MANAGEIMAGESDLG_H
