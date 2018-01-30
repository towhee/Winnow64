#ifndef TOKENDLG_H
#define TOKENDLG_H

#include <QDialog>

namespace Ui {
class TokenDlg;
}

class TokenDlg : public QDialog
{
    Q_OBJECT

public:
    explicit TokenDlg(QStringList &tokenList, QWidget *parent = 0);
    ~TokenDlg();

private:
    Ui::TokenDlg *ui;
};

#endif // TOKENDLG_H
