#ifndef UPDATEAPP_H
#define UPDATEAPP_H

#include <QDialog>
#include <QtWidgets>

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
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();

private:
    Ui::UpdateApp *ui;
    QString &css;
};

#endif // UPDATEAPP_H
