#ifndef ZOOMDLG_H
#define ZOOMDLG_H

#include <QDialog>

namespace Ui {
class ZoomDlg;
}

class ZoomDlg : public QDialog
{
    Q_OBJECT

public:
    explicit ZoomDlg(QWidget *parent = 0);
    ~ZoomDlg();

private:
    Ui::ZoomDlg *ui;
};

#endif // ZOOMDLG_H
