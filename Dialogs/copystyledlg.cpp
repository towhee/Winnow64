#include "copystyledlg.h"
#include "Utilities/utilities.h"
#include "ui_copystyledlg.h"
#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif

CopyStyleDlg::CopyStyleDlg(QString style,
                           QString &name,
                           QStringList templateList,
                           QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CopyStyleDlg),
    name(name)
{
    ui->setupUi(this);
    ui->label->setText("Copy " + Utilities::enquote(style) + " to selected template:");
    // remove 1st item ("Do not Embellish) as connot copy style if no embellishments
    templateList.removeAt(0);
    ui->templateList->addItems(templateList);
    setStyleSheet(G::css);
    #ifdef Q_OS_WIN
        Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif
}

CopyStyleDlg::~CopyStyleDlg()
{
    delete ui;
}

void CopyStyleDlg::on_buttonBox_accepted()
{
    name = ui->templateList->currentText();
    accept();
}

void CopyStyleDlg::on_buttonBox_rejected()
{

}
