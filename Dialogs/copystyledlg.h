#ifndef COPYSTYLEDLG_H
#define COPYSTYLEDLG_H

#include <QDialog>
#include "Main/global.h"

namespace Ui {
class CopyStyleDlg;
}

class CopyStyleDlg : public QDialog
{
    Q_OBJECT

public:
    explicit CopyStyleDlg(QString style, QString &name,
                          QStringList templateList,
                          QWidget *parent = nullptr);
    ~CopyStyleDlg();

private slots:
    void on_buttonBox_accepted();

    void on_buttonBox_rejected();

private:
    Ui::CopyStyleDlg *ui;
    QString &name;
};

#endif // COPYSTYLEDLG_H
