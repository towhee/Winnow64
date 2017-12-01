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
    explicit ZoomDlg(QWidget *parent, int zoom, QRect anchor, QRect c);
    ~ZoomDlg();

public slots:
    void zoomChange(int zoom);
    void positionWindow(QRect a, QRect c);

signals:
    void zoom(qreal zoomVal);
    void updateToggleZoom(qreal toggleZoomVal);

private:
    Ui::ZoomDlg *ui;

private slots:
    void on_zoomSB_valueChanged(int value);
    void on_toggleZoomAmountBtn_clicked();
    void on_radio25Button_clicked();
    void on_radio50Button_clicked();
    void on_radio66Button_clicked();
    void on_radio100Button_clicked();
    void on_radio150Button_clicked();
    void on_radio200Button_clicked();
};

#endif // ZOOMDLG_H
