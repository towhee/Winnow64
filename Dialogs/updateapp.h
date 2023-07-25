#ifndef UPDATEAPP_H
#define UPDATEAPP_H

#include <QDialog>
#include <QtWidgets>
#include "Utilities/win.h"

namespace Ui {
class UpdateApp;
}

class UpdateApp : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateApp(QString &version, QString &css, QWidget *parent = 0);
    ~UpdateApp();

protected:
    void showEvent(QShowEvent *event);

private slots:
    void on_yesBtn_clicked();
    void on_cancelBtn_clicked();

private:
    Ui::UpdateApp *ui;
    QString &css;
};

#endif // UPDATEAPP_H
