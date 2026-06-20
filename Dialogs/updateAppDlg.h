#ifndef UPDATEAPPDLG_H
#define UPDATEAPPDLG_H

#include <QDialog>
#include <QtWidgets>
#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif

namespace Ui {
class UpdateAppDlg;
}

class UpdateAppDlg : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateAppDlg(const QString &newVersion, const QString &currentVersion,
                          const QString &notesUrl, QString &css, QWidget *parent = nullptr);
    ~UpdateAppDlg() override;

    /* True when the user ticked "Don't ask again for this version".  The caller then
       records the offered version so the silent startup check no longer prompts for it. */
    bool skipRequested() const;

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void on_yesBtn_clicked();
    void on_cancelBtn_clicked();

private:
    Ui::UpdateAppDlg *ui;
    QString &css;
};

#endif // UPDATEAPPDLG_H
