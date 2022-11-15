#ifndef ZOOMDLG_H
#define ZOOMDLG_H

#include <QtWidgets>
#include <QDialog>
#include "Main/global.h"

namespace Ui {
class ZoomDlg;
}

class ZoomDlg : public QDialog
{
    Q_OBJECT

public:
    explicit ZoomDlg(QWidget *parent, qreal zoom, QRect anchor, QRect c);
    ~ZoomDlg() override;

public slots:
    void zoomChange(qreal zoom);
    void closeZoomDlg();
    void positionWindow(QRect a, QRect c);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;

signals:
    void zoom(qreal zoomVal);
    void updateToggleZoom(qreal toggleZoomVal);
    void closeZoom();
    void leaveZoom();

private:
    Ui::ZoomDlg *ui;

private slots:
    void on_zoomSB_valueChanged(int value);
    void on_toggleZoomAmountBtn_clicked();
    void on_radio25Button_clicked();
    void on_radio50Button_clicked();
    void on_radio66Button_clicked();
    void on_radio100Button_clicked();
    void on_radio133Button_clicked();
    void on_radio200Button_clicked();
};

#endif // ZOOMDLG_H
