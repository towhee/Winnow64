#include "selectionorpicksdlg.h"
#include "ui_selectionorpicksdlg.h"
#include "Main/global.h"

SelectionOrPicksDlg::SelectionOrPicksDlg(Option &option, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SelectionOrPicksDlg),
    option(option)
{
    ui->setupUi(this);
    setStyleSheet(G::css);
    option = Cancel;

    /*
    A word-wrapped QLabel does not report its wrapped height to the layout, so the
    dialog came up too short and clipped the text. Fix the label width, set its height
    from heightForWidth (after polish, so the G::css font size is in effect) and size
    the dialog to fit. The fixed spacer below the label keeps a gap above the buttons.
    */
    const int labelWidth = 420;
    ui->label->ensurePolished();
    ui->label->setFixedWidth(labelWidth);
    ui->label->setMinimumHeight(ui->label->heightForWidth(labelWidth));
    adjustSize();
    setFixedSize(size());

    #ifdef Q_OS_WIN
        Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif
}

SelectionOrPicksDlg::~SelectionOrPicksDlg()
{
    delete ui;
}

void SelectionOrPicksDlg::on_cancelBtn_clicked()
{
    option = Cancel;
    reject();
}

void SelectionOrPicksDlg::on_usePicksBtn_clicked()
{
    option = Picks;
    accept();
}
void SelectionOrPicksDlg::on_useSelectionBtn_clicked()
{
    option = Selection;
    accept();
}
