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
    PreferencesDlg(Preferences *pref, QString &css, QWidget *parent = nullptr);
private:
};

#endif // PREFERENCESDLG_H
