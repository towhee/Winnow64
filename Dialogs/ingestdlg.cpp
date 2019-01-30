#include "ingestdlg.h"
#include "ui_ingestdlg.h"
//#include "ui_copypickdlg.h"
#include <QDebug>

/*
Files are copied to a destination based on building a file path consisting of:

      Root Folder                   (rootFolderPath)
    + Path to base folder           (fromRootToBaseFolder) source pathTemplateString
    + Base Folder Description       (baseFolderDescription)
    + File Name                     (fileBaseName)     source filenameTemplateString
    + File Suffix                   (fileSuffix)

    ie E:/2018/201802/2018-02-08 Rory birthday/2018-02-08_0001.NEF

    rootFolderPath:         ie "E:/" where all the images are located
    fromRootToBaseFolder:   ie "2018/201802/2018-02-08" from the path template
    baseFolderDescription:  ie " Rory birthday" a description appended to the pathToBaseFolder
    fileBaseName:           ie "2018-02-08_0001" from the filename template
    fileSuffix:             ie ".NEF"

    folderPath:             ie "E:/2018/201802/2018-02-08 Rory birthday/"
        = rootFolderPath + fromRootToBaseFolder + baseFolderDescription + "/"
        = the copy to destination

    The strings fromRootToBaseFolder and the fileBaseName can be tokenized in TokenDlg,
    allowing the user to automate the construction of the entire destination file path.
*/

IngestDlg::IngestDlg(QWidget *parent,
                     bool &combineRawJpg,
                     bool &autoEjectUsb,
                     bool &isBackup,
                     Metadata *metadata,
                     DataModel *dm,
                     QString &ingestRootFolder,
                     QString &ingestRootFolder2,
                     QString &manualFolderPath,
                     QString &manualFolderPath2,
                     QMap<QString, QString>& pathTemplates,
                     QMap<QString, QString>& filenameTemplates,
                     int& pathTemplateSelected,
                     int& pathTemplateSelected2,
                     int& filenameTemplateSelected,
                     QStringList& ingestDescriptionCompleter,
                     bool &isAuto,
                     QString css) :

                     QDialog(parent),
                     ui(new Ui::IngestDlg),
                     isAuto(isAuto),
                     isBackup(isBackup),
                     metadata(metadata),
                     combineRawJpg(combineRawJpg),
                     autoEjectUsb(autoEjectUsb),
                     pathTemplatesMap(pathTemplates),
                     filenameTemplatesMap(filenameTemplates),
                     pathTemplateSelected(pathTemplateSelected),
                     pathTemplateSelected2(pathTemplateSelected2),
                     rootFolderPath(ingestRootFolder),
                     rootFolderPath2(ingestRootFolder2),
                     ingestDescriptionCompleter(ingestDescriptionCompleter),
                     manualFolderPath(manualFolderPath),
                     manualFolderPath2(manualFolderPath2),
                     filenameTemplateSelected(filenameTemplateSelected)
{
    this->dm = dm;

    ui->setupUi(this);
    ui->pathTemplatesCB->setView(new QListView());      // req'd for setting row height in stylesheet
    ui->filenameTemplatesCB->setView(new QListView());  // req'd for setting row height in stylesheet
    setStyleSheet(css);

    isInitializing = true;
    getPicks();

    if (!combineRawJpg) {
        Utilities::setOpacity(ui->combinedIncludeJpgChk, 0.5);
        ui->combinedIncludeJpgChk->setDisabled(true);
    }
    qDebug() << rootFolderPath << rootFolderPath2;
    ui->rootFolderLabel->setText(rootFolderPath);
    ui->rootFolderLabel->setToolTip(ui->rootFolderLabel->text());
    ui->rootFolderLabel_2->setText(rootFolderPath2);
    ui->rootFolderLabel_2->setToolTip(ui->rootFolderLabel_2->text());
    ui->manualFolderLabel->setText(manualFolderPath);
    ui->manualFolderLabel->setToolTip( ui->manualFolderLabel->text());
    ui->manualFolderLabel_2->setText(manualFolderPath2);
    ui->manualFolderLabel_2->setToolTip( ui->manualFolderLabel_2->text());

    // initialize templates and tokens
    initTokenList();
    initExampleMap();
    QMap<QString, QString>::iterator i;
    if (pathTemplatesMap.count() == 0) {
        pathTemplatesMap["YYYY̸YYMM̸YYYY-MM-DD"] = "{YYYY}/{YYYY}{MM}/{YYYY}-{MM}-{DD}";
        pathTemplatesMap["YYYY-MM-DD"] = "{YYYY}-{MM}-{DD}";
    }
    for (i = pathTemplatesMap.begin(); i != pathTemplatesMap.end(); ++i) {
        ui->pathTemplatesCB->addItem(i.key());
        ui->pathTemplatesCB_2->addItem(i.key());
    }
     ui->pathTemplatesCB->setCurrentIndex(pathTemplateSelected);
     ui->pathTemplatesCB_2->setCurrentIndex(pathTemplateSelected2);

    if (filenameTemplatesMap.count() == 0) {
        filenameTemplatesMap["YYYY-MM-DD_XXXX"] = "{YYYY}-{MM}-{DD}_{XXXX}";
        filenameTemplatesMap["Original filename"] = "{ORIGINAL FILENAME}";
    }
    for (i = filenameTemplatesMap.begin(); i != filenameTemplatesMap.end(); ++i)
        ui->filenameTemplatesCB->addItem(i.key());
    ui->filenameTemplatesCB->setCurrentIndex(filenameTemplateSelected);

    // isAuto uses tokens, else use manual dest folder choice
    if (isAuto) {
        ui->descriptionLineEdit->setFocus();
        ui->autoRadio->setChecked(true);
    }
    else {
        ui->selectFolderBtn->setFocus();
        ui->manualRadio->setChecked(true);
    }
    updateEnabledState();

    // assign completer to description
    QCompleter *completer = new QCompleter(this->ingestDescriptionCompleter, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    ui->descriptionLineEdit->setCompleter(completer);
    ui->descriptionLineEdit_2->setCompleter(completer);

    // initialize autoEjectUsb
    ui->ejectChk->setChecked(autoEjectUsb);
    // initialize use backup as well as primary ingest
    ui->backupChk->setChecked(isBackup);

    ui->progressBar->setVisible(false);
    ui->autoIngestTab->tabBar()->setCurrentIndex(0);
    isInitializing = false;
    updateExistingSequence();
    buildFileNameSequence();
}

IngestDlg::~IngestDlg()
{
    delete ui;
}

void IngestDlg::renameIfExists(QString &destination,
                               QString &baseName,
                               QString dotSuffix)
{
    int count = 0;
    bool fileAlreadyExists = true;
    QString newBaseName = baseName + "_";
    do {
        QFile testFile(destination);
        if (testFile.exists()) {
            destination = folderPath + newBaseName + QString::number(++count) + dotSuffix;
            baseName = newBaseName;
        }
        else fileAlreadyExists = false;
    } while (fileAlreadyExists);
}

void IngestDlg::getPicks()
{
/*
The datamodel is sorted by file path. Raw files with the same path precede jpg
files with duplicate names. Two roles track duplicates: G::DupHideRawRole flags
jpg files with duplicate raws and G::DupRawIdxRole points to the duplicate raw
file from the jpg data row.  For example:

Row = 0 "G:/DCIM/100OLYMP/P4020001.ORF" 	DupHideRawRole = true 	DupRawIdxRole = (Invalid)
Row = 1 "G:/DCIM/100OLYMP/P4020001.JPG" 	DupHideRawRole = false 	DupRawIdxRole = QModelIndex(0,0))
Row = 2 "G:/DCIM/100OLYMP/P4020002.ORF" 	DupHideRawRole = true 	DupRawIdxRole = (Invalid)
Row = 3 "G:/DCIM/100OLYMP/P4020002.JPG" 	DupHideRawRole = false 	DupRawIdxRole = QModelIndex(2,0)
*/
    bool inclDupJpg = ui->combinedIncludeJpgChk->isChecked();
    QString fPath;
    pickList.clear();
    for (int row = 0; row < dm->rowCount(); ++row) {
        QModelIndex pickIdx = dm->index(row, G::PickColumn);
        // only picks
        if (pickIdx.data(Qt::EditRole).toString() == "true") {
            QModelIndex idx = dm->index(row, 0);

            // if raw+jpg files have been combined
            if (combineRawJpg) {
                // append if combined jpg and include combined jpgs
                if (inclDupJpg && idx.data(G::DupIsJpgRole).toBool()) {
                    fPath = idx.data(G::PathRole).toString();
                    QFileInfo fileInfo(fPath);
                    pickList.append(fileInfo);
                }
                // append combined raw file
                if (idx.data(G::DupIsJpgRole).toBool()) {
                    idx = qvariant_cast<QModelIndex>(dm->index(row, 0).data(G::DupRawIdxRole));
                }
            }
            fPath = idx.data(G::PathRole).toString();
            QFileInfo fileInfo(fPath);
            pickList.append(fileInfo);
        }
    }

    // stats
    fileCount = pickList.count();
    fileMB = 0;

    qulonglong bytes = 0;
    foreach(QFileInfo info, pickList) bytes += (float)info.size();
    QString s1 = QString::number(fileCount);
    QString s2 = fileCount == 1 ? " file using " : " files using ";
    QString s3 = Utilities::formatMemory(bytes);
    QString s4 = "";
    if (inclDupJpg) s4 = " including duplicate jpg";
    ui->statsLabel->setText(s1 + s2 + s3 + s4);
}

void IngestDlg::ingest()
{
/* The files in pickList are renamed and ingested (copied) from the source
folder to the destination folder.

The destination folder path is appended to the ingest folder history (in the
file menu).

The description, if not "" is appended to the description completer list to
facilitate future ingests.

Each picked image is copied from the source to the destination.

    The destination file base name is based on the file name template selected.
    ie YYYY-MM-DD_xxxx would rename the source file test.jpg to
    2019-01-20_0001.jpg if the seqNum = 1.  The seqNum is incremented with each
    image in the pickList.  This does not happen if the file name template does
    not include the sequence numbering ie use original file.

    If the destination folder already has a file with the same name then _# is
    appended to the destination base file name.

    If there is edited metadata it is written to a sidecar xmp file.

    Finally the source file is copied to the renamed destination.
*/
    bool backup = ui->backupChk->isChecked();

    // get rid of "/" at end of path for history (in file menu)
    QString historyPath = folderPath.left(folderPath.length() - 1);
    emit updateIngestHistory(historyPath);

    QString key = ui->filenameTemplatesCB->currentText();
    QString tokenString;
    if (filenameTemplatesMap.contains(key))
        tokenString = filenameTemplatesMap[key];
    else {
        // add message explaining failure
        return;
    }

    // add description to completer list
    QString desc = ui->descriptionLineEdit->text();
    if (desc.length() > 0) {
        if (!ingestDescriptionCompleter.contains(desc))
            ingestDescriptionCompleter << desc;
    }

    // copy cycles req'd: 1 if no backup, 2 if backup
    int n;
    backup ? n = 2 : n = 1;

    // copy picked images
    ui->progressBar->setVisible(true);
    seqNum =  ui->spinBoxStartNumber->value();
    for (int i = 0; i < pickList.size(); ++i) {
        int progress = (i + 1) * 100 * n / (pickList.size() + 1);
        ui->progressBar->setValue(progress);
        qApp->processEvents();
        QFileInfo fileInfo = pickList.at(i);
        QString sourcePath = fileInfo.absoluteFilePath();

        // seqNum is required by parseTokenString
        // increase sequence unless dup (raw + jpg)
        if (i > 0 && pickList.at(i).baseName() != pickList.at(i-1).baseName())
            seqNum++;

        // rename destination file based on the file naming template
        QString destBaseName =  parseTokenString(pickList.at(i), tokenString);
        QString suffix = fileInfo.suffix().toLower();
        QString dotSuffix = "." + suffix;
        QString destFileName = destBaseName + dotSuffix;

        // buffer to hold file with edited xmp data
        QByteArray buffer;

        // check if image already exists at destination folder
        QString destinationPath = folderPath + destFileName;
        QString backupPath = folderPath2 + destFileName;
        // rename destination and fileName if already exists
        renameIfExists(destinationPath, destBaseName, dotSuffix);

        /* If there is edited xmp data in an eligible file format copy it
           into a buffer.

           This routine is also used in MW::runExternalApp()
        */
        if (metadata->writeMetadata(sourcePath, buffer)
        && metadata->sidecarFormats.contains(suffix)) {
            // copy image file
            QFile::copy(sourcePath, destinationPath);
            if(backup) QFile::copy(sourcePath, backupPath);
             if (metadata->internalXmpFormats.contains(suffix)) {
                // write xmp data into image file       rgh needs some work!!!
                QFile newFile(destinationPath);
                newFile.open(QIODevice::WriteOnly);
                newFile.write(buffer);
                newFile.close();
            }
            else {
                // write the sidecar xmp file
                QFile sidecarFile(folderPath + destBaseName + ".xmp");
                sidecarFile.open(QIODevice::WriteOnly);
                sidecarFile.write(buffer);
                sidecarFile.close();
                if(backup) {
                    QFile sidecarFile(folderPath2 + destBaseName + ".xmp");
                    sidecarFile.open(QIODevice::WriteOnly);
                    sidecarFile.write(buffer);
                    sidecarFile.close();
                }
            }
        }
        // no xmp data, just copy source to destination
        else {
            QFile::copy(sourcePath, destinationPath);
            if(backup) QFile::copy(sourcePath, backupPath);
        }
    }
    QDialog::close();
}

bool IngestDlg::parametersOk()
{
    QString errStr;
    bool err = false;
    bool backup = ui->backupChk->isChecked();

    if(isAuto) {
        if(rootFolderPath.length() == 0) {
            err = true;
            errStr = "The primary location root folder is undefined";
        }

        if(backup) {
            if(rootFolderPath2.length() == 0) {
                err = true;
                errStr += "\nThe backup location root folder is undefined";
            }

            QDir dir2(folderPath2);
            dir2.setFilter(QDir::Files | QDir::NoDotAndDotDot);
            if (dir2.entryInfoList().size() > 0) {
                err = true;
                qDebug() << dir2.entryInfoList().size() << "files";
                errStr += "\nThe automatically selected folder \"" + folderPath2 + "\" already contains "
                          "files.  Backup folders must be empty.";
            }
        }
    }
    else {
        if(manualFolderPath.length() == 0) {
            err = true;
            errStr = "The primary location folder is undefined";
        }

        if(backup) {
            if(manualFolderPath2.length() == 0) {
                err = true;
                errStr += "\nThe backup location folder is undefined";
            }

            // check if already files in backup folder
            QDir dir(manualFolderPath2);
            dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
            qDebug() << dir.entryInfoList().size() << "files";
            if (dir.entryInfoList().size() > 0) {
                err = true;
                errStr += "\nThe manually selected folder \"" + manualFolderPath2 + "\" already contains "
                         "files.  Backup folders must be empty.";
            }
        }
    }

    if(err) {
        QMessageBox::warning(this, tr("Error"), errStr);
        return false;
    }

    return true;
}

void IngestDlg::updateExistingSequence()
{
    if (isInitializing) return;

    QString tokenKey = ui->filenameTemplatesCB->currentText();
    if(tokenKey.length() == 0) return;

    QString tokenString = filenameTemplatesMap[tokenKey];

    // if not a sequence in the token string then disable and return
    if(!tokenString.contains("XX")) {
        Utilities::setOpacity(ui->startSeqLabel, 0.5);
        Utilities::setOpacity(ui->spinBoxStartNumber, 0.5);
        ui->spinBoxStartNumber->setDisabled(true);
        ui->existingSequenceLabel->setVisible(false);
    }
    else {
        // enable sequencing
        Utilities::setOpacity(ui->startSeqLabel, 1.0);
        Utilities::setOpacity(ui->spinBoxStartNumber, 1.0);
        ui->spinBoxStartNumber->setDisabled(false);
        ui->existingSequenceLabel->setVisible(true);
    }

    QDir dir(folderPath);
    if(dir.exists()) {
        int sequenceNum = getSequenceStart(folderPath);
        if(ui->spinBoxStartNumber->value() < sequenceNum + 1)
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
    buildFileNameSequence();
}

void IngestDlg::on_selectFolderBtn_clicked()
{
    QString root = QStandardPaths::displayName(QStandardPaths::HomeLocation);
    QString s;
    s = QFileDialog::getExistingDirectory
        (this, tr("Choose Ingest Primary Folder"), root,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (s.length() > 0) {
        folderPath = s + "/";
        manualFolderPath = folderPath;
        ui->manualFolderLabel->setText(folderPath);
        ui->manualFolderLabel->setToolTip( ui->manualFolderLabel->text());
        buildFileNameSequence();
        updateExistingSequence();
        ui->manualRadio->setChecked(true);
        updateEnabledState();
    }
}

void IngestDlg::on_selectFolderBtn_2_clicked()
{
    QString root = QStandardPaths::displayName(QStandardPaths::HomeLocation);
    QString s;
    s = QFileDialog::getExistingDirectory
        (this, tr("Choose Ingest Backup Folder"), root,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (s.length() > 0) {
        folderPath2 = s + "/";
        manualFolderPath2 = folderPath2;
        ui->manualFolderLabel_2->setText(folderPath2);
        ui->manualFolderLabel_2->setToolTip( ui->manualFolderLabel_2->text());
//        buildFileNameSequence();
//        updateExistingSequence();
        ui->manualRadio->setChecked(true);
        updateEnabledState();
    }
}

void IngestDlg::on_selectRootFolderBtn_clicked()
{
    QString root = QStandardPaths::displayName(QStandardPaths::HomeLocation);
    rootFolderPath = QFileDialog::getExistingDirectory
        (this, tr("Choose Root Folder for Primary Ingest Location"), root,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (rootFolderPath.length() > 0) {
        if(rootFolderPath.right(1) != "/") rootFolderPath += "/";
        ui->rootFolderLabel->setText(rootFolderPath);
        ui->rootFolderLabel->setToolTip(ui->rootFolderLabel->text());
    }

    updateFolderPath();
    updateExistingSequence();
    ui->autoRadio->setChecked(true);
    updateEnabledState();
}

void IngestDlg::on_selectRootFolderBtn_2_clicked()
{
    QString root = QStandardPaths::displayName(QStandardPaths::HomeLocation);
    rootFolderPath2 = QFileDialog::getExistingDirectory
        (this, tr("Choose Root Folder for Backup Location"), root,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (rootFolderPath2.length() > 0) {
        if(rootFolderPath.right(1) != "/") rootFolderPath2 += "/";
        ui->rootFolderLabel_2->setText(rootFolderPath2);
        ui->rootFolderLabel_2->setToolTip(ui->rootFolderLabel_2->text());
    }

    updateFolderPath();
    updateExistingSequence();
    ui->autoRadio->setChecked(true);
    updateEnabledState();
}

bool IngestDlg::isToken(QString tokenString, int pos)
{
    QChar ch = tokenString.at(pos);
    if (ch.unicode() == 8233) return false;  // Paragraph Separator
    if (ch == "{") return false;
    if (pos == 0) return false;

    // look backwards
    bool foundPossibleTokenStart = false;
    int startPos;
    for (int i = pos; i >= 0; i--) {
        ch = tokenString.at(i);
        if (i < pos && ch == "}") return false;
        if (ch == "{") {
            foundPossibleTokenStart = true;
            startPos = i + 1;
        }
        if (foundPossibleTokenStart) break;
    }

    if (!foundPossibleTokenStart) return false;

    // look forwards
    QString token;
//    int n = tokenString.length();
    for (int i = pos; i < tokenString.length(); i++) {
        ch = tokenString.at(i);
        if (ch == "}") {
            for (int j = startPos; j < i; j++) {
                token.append(tokenString.at(j));
            }
            if (exampleMap.contains(token)) {
                currentToken = token;
                tokenStart = startPos - 1;
                tokenEnd = i + 1;
                return true;
            }
        }
    }
    return false;
}

QString IngestDlg::parseTokenString(QFileInfo info,
                                      QString tokenString)
{
    QString fPath = info.absoluteFilePath();
    createdDate = metadata->getCreatedDate(fPath);
    QString s;
    int i = 0;
    while (i < tokenString.length()) {
        if (isToken(tokenString, i + 1)) {
            QString tokenResult;
            // get metadata related to token
            if (currentToken == "YYYY")
                tokenResult = createdDate.date().toString("yyyy");
            if (currentToken == "YY")
                tokenResult = createdDate.date().toString("yy");
            if (currentToken == "MONTH")
                tokenResult = createdDate.date().toString("MMMM").toUpper();
            if (currentToken == "Month")
                tokenResult = createdDate.date().toString("MMMM");
            if (currentToken == "MON")
                tokenResult = createdDate.date().toString("MMM").toUpper();
            if (currentToken == "Mon")
                tokenResult = createdDate.date().toString("MMM");
            if (currentToken == "MM")
                tokenResult = createdDate.date().toString("MM");
            if (currentToken == "DAY")
                tokenResult = createdDate.date().toString("dddd").toUpper();
            if (currentToken == "Day")
                tokenResult = createdDate.date().toString("dddd");
            if (currentToken == "DDD")
                tokenResult = createdDate.date().toString("ddd").toUpper();
            if (currentToken == "Ddd")
                tokenResult = createdDate.date().toString("ddd");
            if (currentToken == "DD")
                tokenResult = createdDate.date().toString("dd");
            if (currentToken == "HOUR")
                tokenResult = createdDate.date().toString("hh");
            if (currentToken == "MINUTE")
                tokenResult = createdDate.date().toString("mm");
            if (currentToken == "SECOND")
                tokenResult = createdDate.date().toString("ss");
            if (currentToken == "TITLE")
                tokenResult = metadata->getTitle(fPath);
            if (currentToken == "CREATOR")
                tokenResult = metadata->getCreator(fPath);
            if (currentToken == "COPYRIGHT")
                tokenResult = metadata->getCopyright(fPath);
            if (currentToken == "ORIGINAL FILENAME")
                tokenResult = info.baseName();
            if (currentToken == "MAKE")
                tokenResult = metadata->getMake(fPath);
            if (currentToken == "MODEL")
                tokenResult = metadata->getModel(fPath);
            if (currentToken == "DIMENSIONS")
                tokenResult = metadata->getDimensions(fPath);
            if (currentToken == "SHUTTER SPEED")
                tokenResult = metadata->getExposureTime(fPath);
            if (currentToken == "APERTURE")
                tokenResult = metadata->getAperture(fPath);
            if (currentToken == "ISO")
                tokenResult = metadata->getISO(fPath);
            if (currentToken == "FOCAL LENGTH")
                tokenResult = metadata->getFocalLength(fPath);
            // sequence from XX to XXXXXXX.  seqNum must be pre-assigned.
            if (currentToken.left(2) == "XX") {
                seqWidth = currentToken.length();
                tokenResult = QString("%1").arg(seqNum, seqWidth , 10, QChar('0'));
            }

            s.append(tokenResult);
            i = tokenEnd;
        }
        else {
            s.append(tokenString.at(i));
            i++;
        }
    }
    return s;
}

void IngestDlg::updateFolderPath()
{
    if (ui->autoRadio->isChecked() || isInitializing) {
        baseFolderDescription = (ui->descriptionLineEdit->text().length() > 0)
                ? ui->descriptionLineEdit->text() : "";
        baseFolderDescription2 = (ui->descriptionLineEdit_2->text().length() > 0)
                ? ui->descriptionLineEdit_2->text() : "";

        folderPath = rootFolderPath + fromRootToBaseFolder + baseFolderDescription + "/";
        ui->folderLabel->setText(folderPath);
        ui->folderLabel->setToolTip(ui->folderLabel->text());

        folderPath2 = rootFolderPath2 + fromRootToBaseFolder2 + baseFolderDescription2 + "/";
        ui->folderLabel_2->setText(folderPath2);
        ui->folderLabel_2->setToolTip(ui->folderLabel_2->text());
    }
    else {
        folderPath = ui->manualFolderLabel->text() + "/";
        folderPath2 = ui->manualFolderLabel_2->text() + "/";
        qDebug() << "folderPath2" << folderPath2;
    }
    updateExistingSequence();
}

void IngestDlg::buildFileNameSequence()
{
/*  This function displays what the ingested file paths will be for the 1st, 2nd and
last file.  If the primary location tab is selected the paths to the primary location
are shown.  If the backup location tab is chosen then the backup location paths are
shown.

*/
    if (isInitializing) return;
    // build filename from tokenString
    QString key = ui->filenameTemplatesCB->currentText();
    QString tokenString;
    if (filenameTemplatesMap.contains(key))
        tokenString = filenameTemplatesMap[key];
    else return;

    // seqNum is required by parseTokenString
    seqStart =  ui->spinBoxStartNumber->value();
    seqNum = seqStart;
    QString ext1 = "." + pickList.at(0).suffix().toUpper();
    QString fileName1 =  parseTokenString(pickList.at(0), tokenString) + ext1;

    QString dirPath;
    // folderpath based on primary or backup tab selected
    qDebug() << "tab index =" << ui->autoIngestTab->tabBar()->currentIndex();
    if(ui->autoIngestTab->tabBar()->currentIndex() == 0) dirPath = folderPath;
    else dirPath = folderPath2;

    if(fileCount == 1) {
        ui->folderPathLabel->setText(dirPath + fileName1);
        ui->folderPathLabel->setToolTip(dirPath + fileName1);
        ui->folderPathLabel_2->setText("");
        ui->folderPathLabel_2->setToolTip("");
        ui->folderPathLabel_3->setText("");
        ui->folderPathLabel_4->setText("");
        ui->folderPathLabel_4->setToolTip("");
        return;
    }

    seqNum++;
    QString ext2 = "." + pickList.at(1).suffix().toUpper();
    QString fileName2 = parseTokenString(pickList.at(1), tokenString) + ext2;
    if(fileCount == 2) {
        ui->folderPathLabel->setText(dirPath + fileName1);
        ui->folderPathLabel->setToolTip(dirPath + fileName1);
        ui->folderPathLabel_2->setText(dirPath + fileName2);
        ui->folderPathLabel_2->setToolTip(dirPath + fileName2);
        ui->folderPathLabel_3->setText("");
        ui->folderPathLabel_4->setText("");
        ui->folderPathLabel_4->setToolTip("");
        return;
    }

    seqNum = seqStart + fileCount - 1;
    QString extN = "." + pickList.at(fileCount - 1).suffix().toUpper();
    QString fileNameN = parseTokenString(pickList.at(fileCount - 1), tokenString) + extN;
    if(fileCount > 2) {
        ui->folderPathLabel->setText(dirPath + fileName1);
        ui->folderPathLabel->setToolTip(dirPath + fileName1);
        ui->folderPathLabel_2->setText(dirPath + fileName2);
        ui->folderPathLabel_2->setToolTip(dirPath + fileName2);
        ui->folderPathLabel_3->setText("...");
        ui->folderPathLabel_4->setText(dirPath + fileNameN);
        ui->folderPathLabel_4->setToolTip(dirPath + fileNameN);
    }
}

void IngestDlg::on_descriptionLineEdit_textChanged(const QString& arg1)
{
    static QString prevText = "";
    updateFolderPath();
    // copy primary description to backup description unless it is already different
    QString desc2 = ui->descriptionLineEdit_2->text();
    qDebug() << "prevText / desc2" << prevText << desc2;
    if (desc2  == prevText) ui->descriptionLineEdit_2->setText(arg1);

    prevText = arg1;
    ui->autoRadio->setChecked(true);
    updateEnabledState();
}

void IngestDlg::on_descriptionLineEdit_2_textChanged(const QString &arg1)
{
    updateFolderPath();
//    ui->folderLabel_2->setText(arg1);
}

void IngestDlg::on_spinBoxStartNumber_valueChanged(const QString /* &arg1 */)
{
    // updateFolderPath();
    int sequenceNum = getSequenceStart(folderPath);
    if(ui->spinBoxStartNumber->value() < sequenceNum)
        ui->spinBoxStartNumber->setValue(sequenceNum + 1);
    buildFileNameSequence();
}

int IngestDlg::getSequenceStart(const QString &path)
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
    }
    return sequence;
}

void IngestDlg::updateEnabledState()
{
    if (isAuto) {
        ui->autoIngestTab->tabBar()->setTabEnabled(0, true);
        ui->autoIngestTab->tabBar()->setTabEnabled(1, true);
        ui->selectRootFolderBtn->setEnabled(true);              //
        ui->templateLabel1->setEnabled(true);
        ui->rootFolderLabel->setEnabled(true);
        ui->rootFolderLabel_2->setEnabled(true);
        ui->pathTemplatesCB->setEnabled(true);                  //
        ui->pathTemplatesBtn->setEnabled(true);                 //
        ui->folderDescription->setEnabled(true);
        ui->descriptionLineEdit->setEnabled(true);              //
        ui->destinationFolderLabel->setEnabled(true);
        ui->folderLabel->setEnabled(true);
        ui->selectRootFolderBtn_2->setEnabled(true);
        ui->templateLabel1_2->setEnabled(true);
        ui->pathTemplatesCB_2->setEnabled(true);
        ui->pathTemplatesBtn_2->setEnabled(true);
        ui->folderDescription_2->setEnabled(true);
        ui->descriptionLineEdit_2->setEnabled(true);
        ui->destinationFolderLabel_2->setEnabled(true);
        ui->folderLabel_2->setEnabled(true);

        ui->selectFolderBtn->setEnabled(false);
        ui->selectFolderBtn_2->setEnabled(false);
        ui->manualFolderLabel->setEnabled(false);
        ui->manualFolderLabel_2->setEnabled(false);
    }
    else {
        ui->autoIngestTab->tabBar()->setTabEnabled(0, false);
        ui->autoIngestTab->tabBar()->setTabEnabled(1, false);
        ui->selectRootFolderBtn->setEnabled(false);
        ui->templateLabel1->setEnabled(false);
        ui->rootFolderLabel->setEnabled(false);
        ui->rootFolderLabel_2->setEnabled(false);
        ui->pathTemplatesCB->setEnabled(false);
        ui->pathTemplatesBtn->setEnabled(false);
        ui->folderDescription->setEnabled(false);
        ui->descriptionLineEdit->setEnabled(false);
        ui->destinationFolderLabel->setEnabled(false);
        ui->folderLabel->setEnabled(false);
        ui->selectRootFolderBtn_2->setEnabled(false);
        ui->templateLabel1_2->setEnabled(false);
        ui->pathTemplatesCB_2->setEnabled(false);
        ui->pathTemplatesBtn_2->setEnabled(false);
        ui->folderDescription_2->setEnabled(false);
        ui->descriptionLineEdit_2->setEnabled(false);
        ui->destinationFolderLabel_2->setEnabled(false);
        ui->folderLabel_2->setEnabled(false);

        ui->selectFolderBtn->setEnabled(true);
        ui->selectFolderBtn_2->setEnabled(true);
        ui->manualFolderLabel->setEnabled(true);
        ui->manualFolderLabel_2->setEnabled(true);
    }
}

void IngestDlg::on_autoRadio_toggled(bool checked)
{
    isAuto = checked;
    updateEnabledState();
    if (isAuto) {
        if (ui->folderLabel->text().length() > 0) {
            updateFolderPath();
            getSequenceStart(folderPath);
            updateExistingSequence();
        }
    }
    else {
        if (ui->manualFolderLabel->text().length() > 0) {
            folderPath = ui->manualFolderLabel->text();
            folderPath2 = ui->manualFolderLabel_2->text();
            buildFileNameSequence();
            updateExistingSequence();
            updateEnabledState();
        }
    }
}

void IngestDlg::on_manualRadio_toggled(bool checked)
{
    isAuto = false;
    if (ui->manualFolderLabel->text().length() > 0) {
        folderPath = ui->manualFolderLabel->text();
        folderPath2 = ui->manualFolderLabel_2->text();
        updateEnabledState();
        buildFileNameSequence();
        updateExistingSequence();
        updateEnabledState();
    }
}


void IngestDlg::initTokenList()
{
/*
The list of tokens in the token editor will appear in this order.
*/
    tokens  << "ORIGINAL FILENAME"
            << "YYYY"
            << "YY"
            << "MONTH"
            << "Month"
            << "MON"
            << "Mon"
            << "MM"
            << "DAY"
            << "Day"
            << "DDD"
            << "Ddd"
            << "DD"
            << "HOUR"
            << "MINUTE"
            << "SECOND"
            << "TITLE"
            << "CREATOR"
            << "COPYRIGHT"
            << "MAKE"
            << "MODEL"
            << "DIMENSIONS"
            << "SHUTTER SPEED"
            << "APERTURE"
            << "ISO"
            << "FOCAL LENGTH"
            << "XX"
            << "XXX"
            << "XXXX"
            << "XXXXX"
            << "XXXXXX"
            << "XXXXXXX"
               ;
}

void IngestDlg::initExampleMap()
{
    exampleMap["ORIGINAL FILENAME"] = "_C8I0024";
    exampleMap["YYYY"] = "2018";
    exampleMap["YY"] = "18";
    exampleMap["MONTH"] = "JANUARY";
    exampleMap["Month"] = "January";
    exampleMap["MON"] = "JAN";
    exampleMap["Mon"] = "Jan";
    exampleMap["MM"] = "01";
    exampleMap["DAY"] = "WEDNESDAY";
    exampleMap["Day"] = "Wednesday";
    exampleMap["DDD"] = "WED";
    exampleMap["Ddd"] = "Wed";
    exampleMap["DD"] = "07";
    exampleMap["HOUR"] = "08";
    exampleMap["MINUTE"] = "32";
    exampleMap["SECOND"] = "45";
    exampleMap["TITLE"] = "Hill_Wedding";
    exampleMap["CREATOR"] = "Rory Hill";
    exampleMap["COPYRIGHT"] = "2018 Rory Hill";
    exampleMap["MAKE"] = "Canon";
    exampleMap["MODEL"] = "Canon EOS-1D X Mark II";
    exampleMap["DIMENSIONS"] = "5472x3648";
    exampleMap["SHUTTER SPEED"] = "1/1000 sec";
    exampleMap["APERTURE"] = "f/5.6";
    exampleMap["ISO"] = "1600";
    exampleMap["FOCAL LENGTH"] = "840 mm";
    exampleMap["XX"] = "01";
    exampleMap["XXX"] = "001";
    exampleMap["XXXX"] = "0001";
    exampleMap["XXXXX"] = "00001";
    exampleMap["XXXXXX"] = "000001";
    exampleMap["XXXXXXX"] = "0000001";
}

void IngestDlg::on_pathTemplatesCB_currentIndexChanged(const QString &arg1)
{
    if (arg1 == "") return;
    QString tokenString = pathTemplatesMap[arg1];
    fromRootToBaseFolder = parseTokenString(pickList.at(0), tokenString);
    if (!isInitializing) pathTemplateSelected = ui->pathTemplatesCB->currentIndex();
    updateFolderPath();
    seqStart = getSequenceStart(folderPath);
    updateExistingSequence();
}

void IngestDlg::on_pathTemplatesCB_2_currentIndexChanged(const QString &arg1)
{
    if (arg1 == "") return;
    QString tokenString = pathTemplatesMap[arg1];
    fromRootToBaseFolder2 = parseTokenString(pickList.at(0), tokenString);
    if (!isInitializing) pathTemplateSelected2 = ui->pathTemplatesCB_2->currentIndex();
    updateFolderPath();
//    seqStart = getSequenceStart(folderPath);
//    updateExistingSequence();
}

void IngestDlg::on_filenameTemplatesCB_currentIndexChanged(const QString &arg1)
{
    if (arg1 == "") return;
    QString tokenString = filenameTemplatesMap[arg1];
    if (!isInitializing) filenameTemplateSelected = ui->filenameTemplatesCB->currentIndex();
    updateExistingSequence();
}

void IngestDlg::on_pathTemplatesBtn_clicked()
{
/*

*/
    // setup TokenDlg
    QString title = "Token Editor - Path from Root to Destination Folder";
    int index = ui->pathTemplatesCB->currentIndex();
    QString currentKey = ui->pathTemplatesCB->currentText();
    TokenDlg *tokenDlg = new TokenDlg(tokens, exampleMap, pathTemplatesMap, index,
                                      currentKey, title, this);
    tokenDlg->exec();

    // rebuild template list and set to same item as TokenDlg for user continuity
    ui->pathTemplatesCB->clear();
    QMap<QString, QString>::iterator i;
    int row = 0;
    for (i = pathTemplatesMap.begin(); i != pathTemplatesMap.end(); ++i) {
        ui->pathTemplatesCB->addItem(i.key());
        if (i.key() == currentKey) index = row;
        row++;
    }
    ui->pathTemplatesCB->setCurrentIndex(index);
    on_pathTemplatesCB_currentIndexChanged(currentKey);
}

void IngestDlg::on_pathTemplatesBtn_2_clicked()
{
/* Performs same function as button on primary tab - just here for convenience and to
make the ui more intuitive
*/
    on_pathTemplatesBtn_clicked();
}

void IngestDlg::on_filenameTemplatesBtn_clicked()
{
    // setup TokenDlg
    // title is also used to filter warnings, so if you change it here also change
    // it in TokenDlg::updateUniqueFileNameWarning
    QString title = "Token Editor - Destination File Name";
    int index = ui->filenameTemplatesCB->currentIndex();
    QString currentKey = ui->filenameTemplatesCB->currentText();
    TokenDlg *tokenDlg = new TokenDlg(tokens, exampleMap, filenameTemplatesMap, index,
                                      currentKey, title, this);
    tokenDlg->exec();

    // rebuild template list and set to same item as TokenDlg for user continuity
    ui->filenameTemplatesCB->clear();
    QMap<QString, QString>::iterator i;
    int row = 0;
    for (i = filenameTemplatesMap.begin(); i != filenameTemplatesMap.end(); ++i) {
        ui->filenameTemplatesCB->addItem(i.key());
        if (i.key() == currentKey) index = row;
        row++;
    }
    ui->filenameTemplatesCB->setCurrentIndex(index);
    on_filenameTemplatesCB_currentIndexChanged(currentKey);
}

void IngestDlg::on_combinedIncludeJpgChk_clicked()

{
    getPicks();
}

void IngestDlg::on_ejectChk_stateChanged(int arg1)
{
    autoEjectUsb = ui->ejectChk->isChecked();
}

void IngestDlg::on_backupChk_stateChanged(int arg1)
{
    isBackup = arg1;
}

void IngestDlg::on_helpBtn_clicked()
{
    QWidget *helpDoc = new QWidget;
    Ui::helpIngest ui;
    ui.setupUi(helpDoc);
    helpDoc->show();
}

void IngestDlg::on_cancelBtn_clicked()
{
    reject();
}

void IngestDlg::on_okBtn_clicked()
{
    // check parameters
    if(!parametersOk()) return;

    bool backup = ui->backupChk->isChecked();

    // make sure the destination folder exists
    QDir dir(folderPath);
    if (!dir.exists()) {
        if(!dir.mkpath(folderPath)) {
            QMessageBox::critical(this, tr("Error"),
                 "The folder \"" + folderPath + "\" was not created.");
            return;
        }
    }

    // make sure the backup folder has been created
    if(backup) {
        QDir dir2(folderPath2);
        if (!dir2.mkpath(folderPath2)) {
            QMessageBox::warning(this, tr("Error"),
                 "The folder \"" + folderPath2 + "\" was not created.");
            return;
        }
    }

    ingest();
}

void IngestDlg::on_autoIngestTab_currentChanged(int index)
{
    buildFileNameSequence();
}
