#include "inputdlg.h"
#include "ui_inputdlg.h"
#include <QDebug>

InputDlg::InputDlg(QString windowTitle,
                   QString description,
                   QString &input,
                   QWidget *parent) :
    QDialog(parent),
    ui(new Ui::InputDlg),
    input(input)
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
    accept();
}

void InputDlg::cancel()
{
    input = "";
    reject();
}
