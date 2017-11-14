#include "copypickdlg.h"
#include "ui_copypickdlg.h"
#include <QDebug>

/*
File naming is based on building a file path consisting of:

    Root Folder + Folder + File Base Name + File Sequence + File Suffix

    Root Folder: where all the images are located YYYY/YYYYMM/
    Folder: where current images are to be copied YYYY-MM-DD_Description
    File Base Name: YYYY-MM-DD (fileNameDatePrefix)
    File Sequence: _XXXX
    File Suffix: ie .NEF
*/
CopyPickDlg::CopyPickDlg(QWidget *parent, QFileInfoList &imageList,
                         Metadata *metadata, QString ingestRootFolder) :
    QDialog(parent),
    ui(new Ui::CopyPickDlg)
{
    ui->setupUi(this);

    pickList = imageList;
    this->metadata = metadata;

    // stats
    fileCount = pickList.count();
    fileMB = 0;
    foreach(QFileInfo info, pickList) fileMB += (float)info.size()/1000000;
    QString s1 = QString::number(fileCount);
    QString s2 = fileCount == 1 ? " file using " : " files using ";
    QString s3 = QString::number(fileMB, 'f', 1);
    ui->statsLabel->setText(s1 + s2 + s3 + "MB");

    fileNameDatePrefix = metadata->getCopyFileNamePrefix(pickList.at(0).absoluteFilePath());
    QString dateTime = metadata->getDateTime(pickList.at(0).absoluteFilePath());
    QString year = dateTime.left(4);
    QString month = dateTime.mid(5,2);

    QString rootFolder = ingestRootFolder;
//    QString rootFolder = "E:/" + year + "/" + year + month + "/";
    ui->parentFolderLabel->setText(rootFolder);
//    ui->folderLabel->setText(fileNameDatePrefix);
    folderPath = "E:/" + year + "/" + year + month + "/" + fileNameDatePrefix;
//    folderPath = "/users/roryhill/pictures/" + fileNameDatePrefix;
    updateFolderPath();
    ui->descriptionLineEdit->setFocus();

    qDebug() << "\ngetSequenceStart()";
    getSequenceStart("");
}

CopyPickDlg::~CopyPickDlg()
{
    delete ui;
}

void CopyPickDlg::accept()
{
    QString destPath = ui->folderPathLabel->text();
    QDir dir;
    dir.mkdir(destPath);
    ui->progressBar->setVisible(true);
    QString prefix = metadata->getCopyFileNamePrefix(pickList.at(0).absoluteFilePath());
    for (int i=0; i < pickList.size(); ++i) {
        int progress = (i+1)*100/(pickList.size()+1);
        ui->progressBar->setValue(progress);
        qApp->processEvents();
        qDebug() << "progressBar->value()" << ui->progressBar->value();
        QFileInfo fileInfo = pickList.at(i);
        QString sequence = "_" + QString("%1").arg(i + 1, 4 , 10, QChar('0'));
        QString suffix = "." + fileInfo.completeSuffix();
        QString source = fileInfo.absoluteFilePath();
        QString destination = destPath + "/" + prefix + sequence + suffix;
        QFile::copy(source, destination);
    }
    QDialog::accept();
}

void CopyPickDlg::updateExistingSequence()
{
    QDir dir(folderPath);
    if(dir.exists()) {
        int sequenceNum = getSequenceStart(folderPath);
        ui->spinBoxStartNumber->setValue(sequenceNum + 1);
        ui->existingSequenceLabel->setText("Folder exists and last image sequence found = " + QString::number(sequenceNum));
    }
}

void CopyPickDlg::on_selectFolderBtn_clicked()
{
    QString root = "/users/roryhill/pictures";
    QString s;
    s = QFileDialog::getExistingDirectory
        (this, tr("Choose Ingest Folder"), root,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (s.length() > 0) {
        folderPath = s;
        ui->folderLabel->setText(folderPath);
    }
    updateFolderPath();
    updateExistingSequence();
}

void CopyPickDlg::on_selectParentFolderBtn_clicked()
{
//    QString root = "E:/2016";
    QString root = "/users/roryhill/pictures";
    QString s;
    s = QFileDialog::getExistingDirectory
        (this, tr("Choose Root Folder"), root,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
//    qDebug() << "Folderpath: " << s;
    if (s.length() > 0) {
        rootFolderPath = s + "/";
        ui->parentFolderLabel->setText(rootFolderPath);
//        folderPath = s + "/" + metadata->getCopyFileNamePrefix(pickList.at(0).absoluteFilePath());
        updateFolderPath();
        updateExistingSequence();
    }
}

void CopyPickDlg::updateFolderPath()
{
    if(ui->parentFolderLabel->text().length() > 0)
        rootFolderPath = ui->parentFolderLabel->text();
    else
        rootFolderPath = defaultRootFolderPath;
    emit updateIngestRootFolder(rootFolderPath);

    folderBase = fileNameDatePrefix;
    folderDescription = (ui->descriptionLineEdit->text().length() > 0)
            ? "_" + ui->descriptionLineEdit->text() : "";

    if(ui->folderLabel->text().length() > 0)
        folderPath = ui->folderLabel->text();
    else
        folderPath = rootFolderPath + folderBase + folderDescription;

    // build filename for result group
    QString prefix = metadata->getCopyFileNamePrefix(pickList.at(0).absoluteFilePath());
    int startNum = ui->spinBoxStartNumber->value();
    QString sequence = "_" + QString("%1").arg(startNum, 4 , 10, QChar('0'));
    QString suffix = "." + pickList.at(0).completeSuffix();
    QString fileName1 = "/" + prefix + sequence + suffix;
    sequence = "_" + QString("%1").arg(startNum + 1, 4 , 10, QChar('0'));
    QString fileName2 = "/" + prefix + sequence + suffix;
    sequence = "_" + QString("%1").arg(startNum + fileCount - 1, 4 , 10, QChar('0'));
    QString fileNameN = "/" + prefix + sequence + suffix;
    ui->folderPathLabel->setStyleSheet("QLabel{color:rgb(180,180,120);}");
    ui->folderPathLabel->setText(folderPath + fileName1);
    ui->folderPathLabel_2->setStyleSheet("QLabel{color:rgb(180,180,120);}");
    ui->folderPathLabel_4->setStyleSheet("QLabel{color:rgb(180,180,120);}");
    if(fileCount > 1) {
        ui->folderPathLabel_2->setText(folderPath + fileName2);
        ui->folderPathLabel_4->setText(folderPath + fileNameN);
    }
    else {
        ui->folderPathLabel_2->setText("");
        ui->folderPathLabel_4->setText(folderPath + fileName1);
    }
//    ui->folderLabel->setText(fileNameDatePrefix + folderDescription);
}

void CopyPickDlg::on_descriptionLineEdit_textChanged(const QString &arg1)
{
    updateFolderPath();
}

void CopyPickDlg::on_spinBoxStartNumber_valueChanged(const QString &arg1)
{
    updateFolderPath();
}

int CopyPickDlg::getSequenceStart(const QString &path)
{
    QDir dir(path);
//    dir.setPath("/users/roryhill/Pictures/2048JPG");
//    dir.setPath(folderPath);
//    if (!dir.exists()) return 0;
    QStringList numbers;
    numbers << "0" << "1" << "2" << "3" << "4" << "5" << "6" << "7" << "8" <<"9";
    QString seq;    // existing number in file
    QString ch;     // one character
    int sequence = 0;
    bool foundNumber;
    for (int f = 0; f < dir.entryList().size(); ++f) {
        seq = "";
        QString fName = dir.entryList().at(f);
        int period = fName.indexOf(".", 0);
        if (period < 1) continue;
        foundNumber = false;
        for (int i = period; i > 0; i--) {
            ch = fName.mid(i, 1);
            if (numbers.contains(ch)) {
                foundNumber = true;
                seq.insert(0, ch);
            }
            else {
                if (foundNumber) {
                    if (seq.toInt() > sequence) sequence = seq.toInt();
                    break;
                }
            }
        }
        qDebug() << fName << fName.indexOf(".", 0) << seq << sequence;
    }
    return sequence;
}
