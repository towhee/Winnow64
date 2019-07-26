#include "preferencesdlg.h"
#include <QDebug>

PreferencesDlg::PreferencesDlg(Preferences *pref, QString &css, QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Winnow Preferences");
    setMinimumSize(470, 500);
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(pref);
    layout->setContentsMargins(0,0,0,0);
    setStyleSheet(css);
}
