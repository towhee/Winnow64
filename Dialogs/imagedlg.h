#ifndef IMAGEDLG_H
#define IMAGEDLG_H

#include <QDialog>

namespace Ui {
class ImageDlg;
}

class ImageDlg : public QDialog
{
    Q_OBJECT

public:
    explicit ImageDlg(QPixmap &pm, QString title, QWidget *parent = nullptr);
    ~ImageDlg() override;

private:
    Ui::ImageDlg *ui;
};

#endif // IMAGEDLG_H