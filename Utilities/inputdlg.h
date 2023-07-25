#ifndef INPUTDLG_H
#define INPUTDLG_H

#include <QDialog>
#include "Main/global.h"
#include "Utilities/win.h"

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
                      QStringList &doNotUse,
                      QWidget *parent = nullptr);

    ~InputDlg();

private slots:
    void checkDoNotUse(QString text);
    void ok();
    void cancel();

private:
    Ui::InputDlg *ui;
    QString &input;
    QStringList &doNotUse;
    bool okToSave = true;
};

#endif // INPUTDLG_H
