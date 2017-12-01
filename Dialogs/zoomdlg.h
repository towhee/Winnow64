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
    explicit ZoomDlg(QWidget *parent, int zoom);
    ~ZoomDlg();

signals:
    void zoom(qreal zoomVal);
    void updateToggleZoom(qreal toggleZoomVal);

private:
    Ui::ZoomDlg *ui;

private slots:
    void on_zoomSB_valueChanged(int value);
    void on_toggleZoomAmountBtn_clicked();
};

#endif // ZOOMDLG_H
