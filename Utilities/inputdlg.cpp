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
    ui->descriptionText->setText(description);
    connect(ui->okBtn, &QPushButton::clicked, this, &InputDlg::ok);
    connect(ui->cancelBtn, &QPushButton::clicked, this, &InputDlg::cancel);
}

InputDlg::~InputDlg()
{
    delete ui;
}

void InputDlg::ok()
{
    input = ui->inputText->text();
    if (doNotUse.contains(ui->inputText->text())) {
        ui->msg->setText();
    }
    accept();
}

void InputDlg::cancel()
{
    input = "";
    reject();
}
