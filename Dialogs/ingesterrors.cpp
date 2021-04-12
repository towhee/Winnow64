#include "ingesterrors.h"
#include "ui_ingesterrors.h"

IngestErrors::IngestErrors(const QStringList &failedToCopy,
                           const QStringList &integrityFailure,
                           QWidget *parent) :
    QDialog(parent),
    ui(new Ui::IngestErrors)
{
    ui->setupUi(this);
    setStyleSheet(G::css);

    if (failedToCopy.length() > 0) {
         ui->textEdit->append("COPY FAILED FOR THESE FILES:\n");
        for (int i = 0; i < failedToCopy.length(); i++) {
             ui->textEdit->append(failedToCopy.at(i));
        }
    }

    if (integrityFailure.length() > 0) {
        ui->textEdit->append("\nINTEGRITY FAILURE FOR THESE FILES:\n");
        for (int i = 0; i < integrityFailure.length(); i++) {
            ui->textEdit->append(integrityFailure.at(i));
        }
    }


}

IngestErrors::~IngestErrors()
{
    delete ui;
}
