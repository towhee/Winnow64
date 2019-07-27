#ifndef PREFERENCESDLG_H
#define PREFERENCESDLG_H

#include <QDialog>
#include <QtWidgets>
#include "Main/global.h"
#include "PropertyEditor/preferences.h"

class PreferencesDlg  : public QDialog
{
    Q_OBJECT

public:
    PreferencesDlg(QWidget *parent, bool &isSolo, Preferences *pref, QString &css);

protected:
    void closeEvent(QCloseEvent *event);

public slots:
    void solo();

private:
    void expand();
    void collapse();
    PropertyEditor *tree;
    QAction *soloAction;
    bool &isSolo;
};

#endif // PREFERENCESDLG_H
