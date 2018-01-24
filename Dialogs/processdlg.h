#ifndef PROCESSDLG_H
#define PROCESSDLG_H

#include <QDialog>
#include <QtWidgets>

namespace Ui {
class Processdlg;
}

class Processdlg : public QDialog
{
    Q_OBJECT

public:
    Processdlg(QMap<QString, QString>& externalApps, QWidget *parent = 0);
    ~Processdlg();

private slots:
    void on_addBtn_clicked();
    void on_removeBtn_clicked();

private:
    Ui::Processdlg *ui;
    QMap<QString, QString>& xApps;
    void reject();
};

#endif // PROCESSDLG_H
