#include "saveasdlg.h"
#include "ui_saveasdlg.h"
#include "Main/global.h"
#include <QDebug>

SaveAsDlg::SaveAsDlg(QModelIndexList &selection,
                     Metadata *metadata,
                     DataModel *dm,
                     QWidget *parent) :

                     QDialog(parent),
                     ui(new Ui::SaveAsDlg),
                     selection(selection),
                     metadata(metadata),
                     dm(dm)
{
    ui->setupUi(this);
    ui->line2->setStyleSheet(
        "QFrame {"
            "border-color:" + G::disabledColor.name() + ";"
            "border-width:0.5px;"
            "border-style:inset;"
        "}");

    QString fPath = selection.at(0).data(G::PathRole).toString();
    QFileInfo info(fPath);
    QString dirPath = info.path() + "/";
    ui->folderPath->setText(dirPath);

    QStringList imageTypes;
    imageTypes << "JPG" << "PNG" << "BMP";
    ui->imageTypesCB->addItems(imageTypes);
    ui->compressionBox->setValue(100);

    ui->progressBar->setVisible(false);

    #ifdef Q_OS_WIN
        Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif
}

SaveAsDlg::~SaveAsDlg()
{
    delete ui;
}

void SaveAsDlg::on_selectBtn_clicked()
{
    QString dirPath = QFileDialog::getExistingDirectory(this, tr("Open Folder"),
         "/home", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    ui->folderPath->setText(dirPath);
}

void SaveAsDlg::on_cancelBtn_clicked()
{
    reject();
}

void SaveAsDlg::on_saveBtn_clicked()
{
    QByteArray ba =  ui->imageTypesCB->currentText().toLocal8Bit();
    const char *imageType = ba.data();
    int compression = ui->compressionBox->value();
    ui->progressBar->setVisible(true);
    for (int i = 0; i < selection.count(); ++i) {
        int progress = (i + 1) * 100 / (selection.count() + 1);
        ui->progressBar->setValue(progress);
        if (G::useProcessEvents) qApp->processEvents();
        QString fPath = selection.at(i).data(G::PathRole).toString();
        QFileInfo info(fPath);
        QString path =  ui->folderPath->text() + "/";
        QString baseName = info.baseName();
        QString dotSuffix = "." + ui->imageTypesCB->currentText();
        QString newFilePath = path + baseName + dotSuffix;
        int count = 0;
        bool fileAlreadyExists = true;
        QString newBaseName = baseName + "_";
        do {
            QFile testFile(newFilePath);
            if (testFile.exists()) {
                newFilePath = path + newBaseName + QString::number(++count) + dotSuffix;
                baseName = newBaseName;
            }
            else fileAlreadyExists = false;
        } while (fileAlreadyExists);

        Pixmap *getImage = new Pixmap(this, dm, metadata);
        QImage image;
        getImage->load(fPath, image, "SaveAsDlg::on_saveBtn_clicked");
        image.save(newFilePath, imageType, compression);
        delete getImage;
    }
    QDialog::accept();
}
