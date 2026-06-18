#ifndef UPDATEAPP_H
#define UPDATEAPP_H

#include <QDialog>
#include <QtWidgets>
#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif

namespace Ui {
class UpdateApp;
}

class UpdateApp : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateApp(const QString &newVersion, const QString &currentVersion,
                       const QString &notesUrl, QString &css, QWidget *parent = nullptr);
    ~UpdateApp();

    /* True when the user pressed "Skip this version".  The caller then records the
       offered version so the silent startup check no longer prompts for it. */
    bool skipRequested() const { return skip; }

protected:
    void showEvent(QShowEvent *event);

private slots:
    void on_yesBtn_clicked();
    void on_cancelBtn_clicked();
    void on_skipBtn_clicked();

private:
    Ui::UpdateApp *ui;
    QString &css;
    bool skip = false;
};

#endif // UPDATEAPP_H
