#ifndef ABOUTDLG_H
#define ABOUTDLG_H

#include <QDialog>
#include <QtWidgets>
#include <QDesktopServices>

namespace Ui {
class About;
}

class AboutDlg : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDlg(QWidget *parent,
                      QString &version,
                      QString &qtVersion,
                      QString &website);
    ~AboutDlg();

private slots:
    void on_okButton_clicked();
    void on_email_linkActivated(const QString &link);
    void on_webSite_linkActivated(const QString &link);

private:
    Ui::About *ui;
};

#endif // ABOUTDLG_H