#ifndef SAVEASDLG_H
#define SAVEASDLG_H

#include <QDialog>
#include <QtWidgets>
#include "Image/pixmap.h"

namespace Ui {
class SaveAsDlg;
}

class SaveAsDlg : public QDialog
{
    Q_OBJECT

public:
    explicit SaveAsDlg(QModelIndexList &selection,
                       Metadata *metadata,
                       DataModel *dm,
                       QWidget *parent = nullptr);
    ~SaveAsDlg();

private slots:
    void on_selectBtn_clicked();

    void on_cancelBtn_clicked();

    void on_saveBtn_clicked();

private:
    Ui::SaveAsDlg *ui;
    QModelIndexList &selection;
    Metadata *metadata;
    DataModel *dm;
};

#endif // SAVEASDLG_H
