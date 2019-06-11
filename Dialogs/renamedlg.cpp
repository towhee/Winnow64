#include "renamedlg.h"
#include "ui_renamedlg.h"

/*******************************************************************************
   RenameEdit Class
*******************************************************************************/

/* RenameEdit is a subclass of QTextEdit to manage tokenized text. It tokenizes
dragged text in insertFromMimeData and has a parse function to use the tokens
to look up corresponding metadata.

  */

RenameEdit::RenameEdit(QDialog *parent) : QLineEdit(parent)
{
    this->parent = parent;
}

void RenameEdit::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Slash:
    case Qt::Key_Backslash:
        G::popUp->show("The characters \"\\\" and \"/\" are not permitted");
        return;
    }
    QLineEdit::keyPressEvent(event);
}

/*******************************************************************************
   RenameDlg Class
*******************************************************************************/

RenameDlg::RenameDlg(QString &name,
                     QStringList &existingNames,
                     QString dlgTitle,
                     QString nameTitle,
                     QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RenameDlg),
    name(name),
    existingNames(existingNames)
{
    ui->setupUi(this);
    setWindowTitle(dlgTitle);
    ui->nameLbl->setText(nameTitle);
    ui->name->setText(name);

    QString existingNamesString = "Existing Names: \n";
    for (int i = 0; i < existingNames.count(); ++i) {
        existingNamesString.append(existingNames.at(i) + "\n");
    }
    ui->name->setToolTip(existingNamesString);
}

RenameDlg::~RenameDlg()
{
    delete ui;
}

void RenameDlg::on_name_textChanged(const QString &text)
{
    if (existingNames.contains(text)) {
        ui->name->setStyleSheet("QLineEdit {color:red;}");
        ui->statusLbl->setText("Warning: duplicate name.  Please make unique.");
        isUnique = false;
    }
    else {
        ui->name->setStyleSheet("QLineEdit {color:rgb(229,229,229);}");
        ui->statusLbl->setText("");
        isUnique = true;
    }
}

void RenameDlg::on_okBtn_clicked()
{
    if (isUnique && ui->name->text().length() > 0) {
        name = ui->name->text();
        accept();
        return;
    }
    reject();
}

void RenameDlg::on_cancelBtn_clicked()
{
    reject();
}
