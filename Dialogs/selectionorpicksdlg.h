#ifndef SELECTIONORPICKSDLG_H
#define SELECTIONORPICKSDLG_H

#include <QDialog>
#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif

namespace Ui {
class SelectionOrPicksDlg;
}

class SelectionOrPicksDlg : public QDialog
{
    Q_OBJECT

public:
    enum Option {Cancel, Picks, Selection};
    explicit SelectionOrPicksDlg(Option &option, QWidget *parent = nullptr);
    ~SelectionOrPicksDlg();

private slots:
    void on_cancelBtn_clicked();
    void on_usePicksBtn_clicked();
    void on_useSelectionBtn_clicked();

private:
    Ui::SelectionOrPicksDlg *ui;
    Option &option;
};

#endif // SELECTIONORPICKSDLG_H
