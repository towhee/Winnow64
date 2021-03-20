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

signals:
    void itemRenamed(QString oldName, QString newName);

public slots:
    void save(QPixmap *src);

private slots:
    void itemDoubleClicked(QTableWidgetItem *item);
    void itemChanged(QTableWidgetItem *item);
    void on_newBtn_clicked();
    void on_deleteBtn_clicked();
    void on_doneBtn_clicked();

private:
    void extractTile(QPixmap &src);
    QString templatesUsingTile(QString name);
    QString templatesUsingGraphic(QString name);
    Ui::ManageImagesDlg *ui;
    QSettings *setting;
    QString settingPath;
    QString fPath;
    int ht;
    bool isInitializing = true;
    QString prevName;
};

#endif // MANAGEIMAGESDLG_H
