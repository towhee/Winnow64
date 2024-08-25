#include "loupeinfodlg.h"
#include "ui_loupeinfodlg.h"

LoupeInfoDlg::LoupeInfoDlg(QStringList &tokens,
                           QMap<QString, QString> &exampleMap,
                           QMap<QString, QString> &infoTemplates,
                           QMap<QString, QString> &usingTokenMap,
                           int &index,
                           QString &currentKey,
                           std::function<void ()> updateInfoCallback,
                           QWidget *parent) :

                           QDialog(parent),
                           ui(new Ui::LoupeInfoDlg),
                           tokens(tokens),
                           exampleMap(exampleMap),
                           infoTemplates(infoTemplates),
                           usingTokenMap(usingTokenMap),
                           index(index),
                           currentKey(currentKey),
                           updateShootingInfoCallback(updateInfoCallback)
{
    if (G::isLogger) G::log("LoupeInfoDlg::LoupeInfoDlg");
    ui->setupUi(this);

    QMap<QString, QString>::iterator i;
    for (i = infoTemplates.begin(); i != infoTemplates.end(); ++i) {
        ui->templatesCB->addItem(i.key());
    }
    isInitializing = false;
    ui->templatesCB->setCurrentIndex(index);
    ui->templatesCB->setCurrentText(currentKey);
}

LoupeInfoDlg::~LoupeInfoDlg()
{
    delete ui;
}

void LoupeInfoDlg::on_templateEditorBtn_clicked()
{
    if (G::isLogger) G::log("LoupeInfoDlg::on_templateEditorBtn_clicked");
    QString title = "Loupe View Info Token Editor";
    TokenDlg tokenDlg(tokens, exampleMap, infoTemplates, usingTokenMap, index,
                      currentKey, title, this);
    tokenDlg.exec();
}

void LoupeInfoDlg::on_okBtn_clicked()
{
    accept();
}

void LoupeInfoDlg::on_templatesCB_currentIndexChanged(int idx)
{
    if (G::isLogger) G::log("LoupeInfoDlg::on_templatesCB_currentIndexChanged");
    if (isInitializing) return;
    index = idx;
    currentKey = ui->templatesCB->currentText();
    // callback
    if (updateShootingInfoCallback) {
        updateShootingInfoCallback();
    }
}

