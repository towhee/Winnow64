#include "copypickdlg.h"
#include "ui_copypickdlg.h"
#include <QDebug>

/*
File naming is based on building a file path consisting of:

    Root Folder + Path to base folder + File Base Name + File Sequence + File Suffix

    Root Folder: where all the images are located ie e:\ or users/pictures
    PathToBaseFolder: ie e:/YYYY/YYMM
    Folder: where current images are to be copied YYYY-MM-DD_Description
    File Base Name: YYYY-MM-DD (fileNameDatePrefix)
    File Sequence: _XXXX
    File Suffix: ie .NEF
*/
CopyPickDlg::CopyPickDlg(QWidget *parent, QFileInfoList &imageList,
                         Metadata *metadata, QString ingestRootFolder,
                         bool isAuto) :
    QDialog(parent),
    ui(new Ui::CopyPickDlg)
{
    ui->setupUi(this);

    isInitializing = true;
    this->isAuto = isAuto;

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

    // get year and month from the first image
    fileNameDatePrefix = metadata->getCopyFileNamePrefix(pickList.at(0).absoluteFilePath());
    created = metadata->getCreated(pickList.at(0).absoluteFilePath());
    year = created.left(4);
    month = created.mid(5,2);

    rootFolderPath = ingestRootFolder;
    ui->rootFolderLabel->setText(rootFolderPath);

    pathToBaseFolder = rootFolderPath + year + "/" + year + month + "/" ;
    ui->parentFolderLabel->setText(pathToBaseFolder);

    updateFolderPath();
    getSequenceStart(folderPath);
    updateExistingSequence();

    if (isAuto) {
        ui->descriptionLineEdit->setFocus();
        ui->autoRadio->setChecked(true);
    }
    else {
        ui->selectFolderBtn->setFocus();
        ui->manualRadio->setChecked(true);
    }
    isInitializing = false;
}

CopyPickDlg::~CopyPickDlg()
{
    delete ui;
}

void CopyPickDlg::accept()
{
    QDir dir(folderPath);
    if (!dir.exists()) {
        if(!dir.mkpath(folderPath)) {
            QMessageBox::critical(this, tr("Error"),
                 "The folder (" + folderPath + ") was not created.");
            QDialog::accept();
            return;
        }
    }
    ui->progressBar->setVisible(true);
    QString prefix = metadata->getCopyFileNamePrefix(pickList.at(0).absoluteFilePath());

    for (int i=0; i < pickList.size(); ++i) {
        int progress = (i+1)*100/(pickList.size()+1);
        ui->progressBar->setValue(progress);
        qApp->processEvents();
        qDebug() << "progressBar->value()" << ui->progressBar->value();
        QFileInfo fileInfo = pickList.at(i);
        int seqNum = ui->spinBoxStartNumber->value() + i;
        QString sequence = "_" + QString("%1").arg(seqNum, 4 , 10, QChar('0'));
        QString suffix = "." + fileInfo.completeSuffix();
        QString source = fileInfo.absoluteFilePath();
        QString destination = folderPath + "/" + prefix + sequence + suffix;
        qDebug() << "Copying " << source << " to " << destination;
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
        if (sequenceNum > 0)
            ui->existingSequenceLabel->setText("Folder exists and last image sequence found = "
                                               + QString::number(sequenceNum));
        else
            ui->existingSequenceLabel->setText("Folder exists but no sequenced images found");
    }
    else {
        ui->spinBoxStartNumber->setValue(1);
        ui->existingSequenceLabel->setText("");
    }
}

void CopyPickDlg::on_selectFolderBtn_clicked()
{
    qDebug() << "CopyPickDlg::on_selectFolderBtn_clicked";
    QString root = QStandardPaths::displayName(QStandardPaths::HomeLocation);
    QString s;
    s = QFileDialog::getExistingDirectory
        (this, tr("Choose Ingest Folder"), root,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (s.length() > 0) {
        folderPath = s;
        ui->manualFolderLabel->setText(folderPath);
    }
    buildFileNameSequence();
    updateExistingSequence();
    ui->manualRadio->setChecked(true);
    updateStyleOfFolderLabels();
}

void CopyPickDlg::on_selectRootFolderBtn_clicked()
{
    QString root = QStandardPaths::displayName(QStandardPaths::HomeLocation);
//    QString rootFolderPath;
    rootFolderPath = QFileDialog::getExistingDirectory
        (this, tr("Choose Root Folder"), root,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (rootFolderPath.length() > 0) {
        ui->rootFolderLabel->setText(rootFolderPath);
    }

    // send to MW where it will be saved in QSettings
    emit updateIngestParameters(rootFolderPath, isAuto);

    pathToBaseFolder = rootFolderPath + year + "/" + year + month + "/";
    ui->parentFolderLabel->setText(pathToBaseFolder);

    updateFolderPath();
    updateExistingSequence();
    ui->autoRadio->setChecked(true);
    updateStyleOfFolderLabels();
}

void CopyPickDlg::updateFolderPath()
{
    if (ui->autoRadio->isChecked() || isInitializing) {
        folderBase = fileNameDatePrefix;

        folderDescription = (ui->descriptionLineEdit->text().length() > 0)
                ? "_" + ui->descriptionLineEdit->text() : "";

        folderPath = pathToBaseFolder + folderBase + folderDescription;
        ui->folderLabel->setText(folderPath);
    }
    else {
        folderPath = ui->manualFolderLabel->text();
    }

    buildFileNameSequence();
}

void CopyPickDlg::buildFileNameSequence()
{
    // build filename for result group YYYY-MM-DD_XXXX.SUFFIX

    // file prefix YYYY-MM-DD comes prebuilt from Metadata class
    QString prefix = metadata->getCopyFileNamePrefix(pickList.at(0).absoluteFilePath());
    int startNum = ui->spinBoxStartNumber->value();
    QString sequence = "_" + QString("%1").arg(startNum, 4 , 10, QChar('0'));
    QString suffix = "." + pickList.at(0).completeSuffix();
    QString fileName1 = "/" + prefix + sequence + suffix;
    sequence = "_" + QString("%1").arg(startNum + 1, 4 , 10, QChar('0'));
    QString fileName2 = "/" + prefix + sequence + suffix;
    sequence = "_" + QString("%1").arg(startNum + fileCount - 1, 4 , 10, QChar('0'));
    QString fileNameN = "/" + prefix + sequence + suffix;
    ui->folderPathLabel->setText(folderPath + fileName1);
    ui->folderPathLabel->setToolTip(folderPath + fileName1);
    if(fileCount > 1) {
        ui->folderPathLabel_2->setText(folderPath + fileName2);
        ui->folderPathLabel_2->setToolTip(folderPath + fileName2);
        ui->folderPathLabel_4->setText(folderPath + fileNameN);
        ui->folderPathLabel_4->setToolTip(folderPath + fileNameN);
    }
    else {
        ui->folderPathLabel_2->setText("");
        ui->folderPathLabel_2->setToolTip("");
        ui->folderPathLabel_4->setText(folderPath + fileName1);
        ui->folderPathLabel_4->setToolTip(folderPath + fileName1);
    }
}

void CopyPickDlg::on_descriptionLineEdit_textChanged(const QString& /*arg1*/)
{
    updateFolderPath();
    ui->autoRadio->setChecked(true);
    updateStyleOfFolderLabels();
}

void CopyPickDlg::on_spinBoxStartNumber_valueChanged(const QString &arg1)
{
    arg1.isEmpty();             // suppress compiler warning
    updateFolderPath();
}

int CopyPickDlg::getSequenceStart(const QString &path)
{
    QDir dir(path);
    if (!dir.exists()) return 0;

    // filter on images only
    QStringList fileFilters;
    foreach (const QString &str, metadata->supportedFormats)
            fileFilters.append("*." + str);
    dir.setNameFilters(fileFilters);
    dir.setFilter(QDir::Files);

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

void CopyPickDlg::updateStyleOfFolderLabels()
{
    if (ui->autoRadio->isChecked()) {
        ui->folderLabel->setStyleSheet("QLabel{color:rgb(180,180,120);}");
        ui->manualFolderLabel->setStyleSheet("QLabel{color:rgb(229,229,229);}");
    }
    else {
        ui->folderLabel->setStyleSheet("QLabel{color:rgb(229,229,229);}");
        ui->manualFolderLabel->setStyleSheet("QLabel{color:rgb(180,180,120);}");
    }
}

void CopyPickDlg::on_autoRadio_toggled(bool checked)
{
    if (checked) {      // auto
        isAuto = true;
        if (ui->folderLabel->text().length() > 0) {
            updateFolderPath();
            updateStyleOfFolderLabels();
            getSequenceStart(folderPath);
            updateExistingSequence();
        }
    }
    else {              // manual
        isAuto = false;
        if (ui->manualFolderLabel->text().length() > 0) {
            folderPath = ui->manualFolderLabel->text();
            buildFileNameSequence();
            updateExistingSequence();
            updateStyleOfFolderLabels();
        }
    }
    emit updateIngestParameters(rootFolderPath, isAuto);
}

void CopyPickDlg::on_tokenEditorBtn_clicked()
{
    QMap<QString, QString> tokenMap;
    tokenMap["YYYY"] = "2018";
    tokenMap["YY"] = "18";
    tokenMap["MONTH"] = "JANUARY";
    tokenMap["Month"] = "January";
    tokenMap["MON"] = "JAN";
    tokenMap["Mon"] = "Jan";
    tokenMap["MM"] = "01";
    tokenMap["DAY"] = "WEDNESDAY";
    tokenMap["Day"] = "Wednesday";
    tokenMap["DDD"] = "WED";
    tokenMap["Ddd"] = "Wed";
    tokenMap["DD"] = "07";
    tokenMap["TITLE"] = "Hill_Wedding";
    tokenMap["AUTHOR"] = "Rory Hill";
    tokenMap["COPYRIGHT"] = "2018 Rory Hill";

//    QStringList pathTokens;
//    pathTokens << "YYYY"
//               << "YY"
//               << "MONTH"
//               << "MM"
//               << "DAY"
//               << "DDD"
//               << "DD"
//               << "TITLE"
//               << "AUTHOR"
//               << "COPYRIGHT";

    TokenDlg *tokenDlg = new TokenDlg(tokenMap, this);
    tokenDlg->exec();
}
