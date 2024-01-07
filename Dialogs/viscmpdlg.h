#ifndef VISCMPDLG_H
#define VISCMPDLG_H

#include <QDialog>

namespace Ui {
class VisCmpDlg;
}

class VisCmpDlg : public QDialog
{
    Q_OBJECT

public:
    explicit VisCmpDlg(QWidget *parent = nullptr);
    ~VisCmpDlg();

private:
    Ui::VisCmpDlg *ui;
};

#endif // VISCMPDLG_H
