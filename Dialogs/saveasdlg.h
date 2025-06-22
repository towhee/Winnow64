#ifndef SAVEASDLG_H
#define SAVEASDLG_H

#include <QDialog>
#include <QtWidgets>
#include "Image/pixmap.h"
#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif

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
    ~SaveAsDlg() override;
    QString getFolderPath();

private slots:
    void on_selectBtn_clicked();
    void on_cancelBtn_clicked();
    void on_saveBtn_clicked();

private:
    Ui::SaveAsDlg *ui;
    QModelIndexList &selection;
    Metadata *metadata;
    DataModel *dm;
    QString savedFolderPath;
};

#endif // SAVEASDLG_H
