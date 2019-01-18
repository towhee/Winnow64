#ifndef UPDATEAPP_H
#define UPDATEAPP_H

#include <QDialog>

namespace Ui {
class UpdateApp;
}

class UpdateApp : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateApp(QString &notes, QWidget *parent = 0);
    ~UpdateApp();

private slots:
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();

private:
    Ui::UpdateApp *ui;
};

#endif // UPDATEAPP_H
