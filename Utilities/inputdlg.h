#ifndef INPUTDLG_H
#define INPUTDLG_H

#include <QDialog>
#include "Main/global.h"

namespace Ui {
class InputDlg;
}

class InputDlg : public QDialog
{
    Q_OBJECT

public:
    explicit InputDlg(QString windowTitle,
                      QString description,
                      QString &input,
                      QWidget *parent = nullptr);

    ~InputDlg();

private slots:
    void ok();
    void cancel();

private:
    Ui::InputDlg *ui;
    QString &input;
};

#endif // INPUTDLG_H
