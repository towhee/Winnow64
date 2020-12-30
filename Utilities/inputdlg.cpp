#include "inputdlg.h"
#include "ui_inputdlg.h"
#include <QDebug>

InputDlg::InputDlg(QString windowTitle,
                   QString description,
                   QString &input,
                   QStringList &doNotUse,
                   QWidget *parent) :
    QDialog(parent),
    ui(new Ui::InputDlg),
    input(input),
    doNotUse(doNotUse)
{
    ui->setupUi(this);
    setWindowTitle(windowTitle);
    setStyleSheet(G::css);
    ui->inputText->setText(input);
    ui->descriptionText->setText(description);
    if (doNotUse.contains(input)) doNotUse.removeAt(doNotUse.indexOf(input));
    connect(ui->inputText, &QLineEdit::textChanged, this, &InputDlg::checkDoNotUse);
    connect(ui->okBtn, &QPushButton::clicked, this, &InputDlg::ok);
    connect(ui->cancelBtn, &QPushButton::clicked, this, &InputDlg::cancel);
}

InputDlg::~InputDlg()
{
    delete ui;
}

void InputDlg::checkDoNotUse(QString text)
{
    if (doNotUse.contains(text.trimmed())) {
        ui->msg->setText("Duplicate text.  Please try again.");
        okToSave = false;
    }
    else ui->msg->setText("");
}

void InputDlg::ok()
{
    input = ui->inputText->text().trimmed();
//    if (doNotUse.contains(input)) {
//        ui->msg->setText("Duplicate text.  Please try again.");
//        return;
//    }
    if (okToSave) accept();
}

void InputDlg::cancel()
{
    input = "";
    reject();
}
