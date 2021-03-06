#ifndef RENAMEDLG_H
#define RENAMEDLG_H

#include <QtWidgets>
#include "Main/global.h"

/*****************************************************************************/
class RenameEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit RenameEdit(QDialog *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    QDialog *parent;
};

/*****************************************************************************/
namespace Ui {
class RenameDlg;
}

class RenameDlg : public QDialog
{
    Q_OBJECT

public:
    explicit RenameDlg(QString &name,
                       QStringList &existingNames,
                       QString dlgTitle,
                       QString nameTitle,
                       QWidget *parent = nullptr);
    ~RenameDlg();

private slots:
    void on_name_textChanged(const QString &text);

    void on_okBtn_clicked();

    void on_cancelBtn_clicked();

private:
    Ui::RenameDlg *ui;
    QString &name;
    QStringList &existingNames;
    bool isUnique;
};

#endif // RENAMEDLG_H
