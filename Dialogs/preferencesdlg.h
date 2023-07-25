#ifndef PREFERENCESDLG_H
#define PREFERENCESDLG_H

#include <QDialog>
#include <QtWidgets>
#include "Main/global.h"
#include "PropertyEditor/preferences.h"
#include "Utilities/win.h"

class PreferencesDlg  : public QDialog
{
    Q_OBJECT

public:
    PreferencesDlg(QWidget *parent, bool &isSolo, Preferences *pref, QString &css);
    ~PreferencesDlg() override;

public slots:
    void solo();

private:
    PropertyEditor *tree;
    void expand();
    void collapse();
    QHBoxLayout* layout;
    QAction *expandAllAction;
    QAction *collapseAllAction;
    QAction *soloAction;
    bool &isSolo;
};

#endif // PREFERENCESDLG_H
