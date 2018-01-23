#ifndef PROCESSDLG_H
#define PROCESSDLG_H

#include <QDialog>

namespace Ui {
class processdlg;
}

class processdlg : public QDialog
{
    Q_OBJECT

public:
    explicit processdlg(QWidget *parent = 0);
    ~processdlg();

private slots:
    void on_addBtn_clicked();

    void on_removeBtn_clicked();

private:
    Ui::processdlg *ui;
};

#endif // PROCESSDLG_H
