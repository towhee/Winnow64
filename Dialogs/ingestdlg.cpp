#include "ingestdlg.h"
#include "ui_ingestdlg.h"
//#include "ui_copypickdlg.h"
#include <QDebug>

/*
When this dialog is invoked the files that have been picked are copied to a primary
destination folder, and optionally, to a backup location.  This process is known as
ingestion: selecting and copying images froma camera card to a computer.

The destination folder can be selected/created manually or automatically.  If
automatic, a root folder is selected/created, a path to the destination folder
is defined using tokens, and the destination folder description defined.  The
picked images can be renamed during this process.

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
                     bool &integrityCheck,
                     bool &ingestIncludeXmpSidecar,
                     bool &isBackup,
                     bool &gotoIngestFolder,
                     Metadata *metadata,
                     DataModel *dm,
                     QString &ingestRootFolder,
                     QString &ingestRootFolder2,
                     QString &manualFolderPath,
                     QString &manualFolderPath2,
                     QString &baseFolderDescription,
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
                     metadata(metadata),
                     isAuto(isAuto),
                     combineRawJpg(combineRawJpg),
                     autoEjectUsb(autoEjectUsb),
                     integrityCheck(integrityCheck),
                     ingestIncludeXmpSidecar(ingestIncludeXmpSidecar),
                     isBackup(isBackup),
                     gotoIngestFolder(gotoIngestFolder),
                     pathTemplatesMap(pathTemplates),
                     filenameTemplatesMap(filenameTemplates),
                     pathTemplateSelected(pathTemplateSelected),
                     pathTemplateSelected2(pathTemplateSelected2),
                     rootFolderPath(ingestRootFolder),
                     rootFolderPath2(ingestRootFolder2),
                     baseFolderDescription(baseFolderDescription),
                     ingestDescriptionCompleter(ingestDescriptionCompleter),
                     manualFolderPath(manualFolderPath),
                     manualFolderPath2(manualFolderPath2),
                     filenameTemplateSelected(filenameTemplateSelected)
{
    if (G::isLogger) G::log(__FUNCTION__); 
    this->dm = dm;
    this->css = css;

    QString radioStyle =
        "QRadioButton:checked {"
            "color: yellow;"
        "}"
        "QRadioButton:unchecked {"
            "color: green;"
        "}";
//    ui->autoRadio->setStyleSheet(radioStyle);
//    ui->manualRadio->setStyleSheet(radioStyle);

    ui->setupUi(this);
    setStyleSheet(css);

    ui->pathTemplatesCB->setView(new QListView());      // req'd for setting row height in stylesheet
    ui->filenameTemplatesCB->setView(new QListView());  // req'd for setting row height in stylesheet

    isInitializing = true;

    // populate the pick list
    getPicks();

    if (!combineRawJpg) {
        Utilities::setOpacity(ui->combinedIncludeJpgChk, 0.5);
        ui->combinedIncludeJpgChk->setDisabled(true);
    }
    ui->rootFolderLabel->setText(rootFolderPath);
    ui->rootFolderLabel->setToolTip(ui->rootFolderLabel->text());
    ui->rootFolderLabel_2->setText(rootFolderPath2);
    ui->rootFolderLabel_2->setToolTip(ui->rootFolderLabel_2->text());
    ui->manualFolderLabel->setText(manualFolderPath);
    ui->manualFolderLabel->setToolTip(ui->manualFolderLabel->text());
    ui->manualFolderLabel_2->setText(manualFolderPath2);
    ui->manualFolderLabel_2->setToolTip(ui->manualFolderLabel_2->text());

    ui->descriptionLineEdit->setText(baseFolderDescription);

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
    if (baseFolderDescription.length() > 0) {
        ui->okBtn->setDefault(true);
        ui->okBtn->setFocus();
    }
    updateEnabledState();

    // assign completer to description
    QCompleter *completer = new QCompleter(this->ingestDescriptionCompleter, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    ui->descriptionLineEdit->setCompleter(completer);
    ui->descriptionLineEdit_2->setCompleter(completer);

    // initialize autoEjectUsb
    ui->ejectChk->setChecked(autoEjectUsb);
    // initialize integrityCheck
    ui->integrityChk->setChecked(integrityCheck);
    // initialize ingestIncludeXmpSidecar
    ui->includeXmpChk->setChecked(ingestIncludeXmpSidecar);
    // initialize use backup as well as primary ingest
    ui->backupChk->setChecked(isBackup);
//    isBackupChkBox->setChecked(isBackup);
    // initialize open ingest folder after ingest
    ui->openIngestFolderChk->setChecked(gotoIngestFolder);

    ui->progressBar->setVisible(false);
    ui->autoIngestTab->tabBar()->setCurrentIndex(0);
    isInitializing = false;
    updateExistingSequence();
    buildFileNameSequence();

//    connect(isBackupChkBox, SIGNAL(stateChanged(int)),
//            this, SLOT(on_isBackupChkBox_stateChanged(int)));
}

IngestDlg::~IngestDlg()
{
    delete ui;
}

void IngestDlg::renameIfExists(QString &destination,
                               QString &baseName,
                               QString dotSuffix)
{
    if (G::isLogger) G::log(__FUNCTION__); 
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
    This function generates a list of picked images to ingest from the filtered data. The
    datamodel dm, not the proxy dm->sf, is used, as the user may have sorted the view, and we
    need the original datamodel dm order to efficiently deal with the combined raw/jpg
    scenario.

    The datamodel is sorted by file path. Raw files with the same path precede jpg files with
    duplicate names. Two roles track duplicates: G::DupHideRawRole flags jpg files with
    duplicate raws and G::DupRawIdxRole points to the duplicate raw file from the jpg data
    row. For example:

    Row = 0 "G:/DCIM/100OLYMP/P4020001.ORF" 	DupHideRawRole = true 	DupRawIdxRole = (Invalid)
    Row = 1 "G:/DCIM/100OLYMP/P4020001.JPG" 	DupHideRawRole = false 	DupRawIdxRole = QModelIndex(0,0))
    Row = 2 "G:/DCIM/100OLYMP/P4020002.ORF" 	DupHideRawRole = true 	DupRawIdxRole = (Invalid)
    Row = 3 "G:/DCIM/100OLYMP/P4020002.JPG" 	DupHideRawRole = false 	DupRawIdxRole = QModelIndex(2,0)
*/
    if (G::isLogger) G::log(__FUNCTION__); 
    bool inclDupJpg = ui->combinedIncludeJpgChk->isChecked();
    QString fPath;
    pickList.clear();
    for (int row = 0; row < dm->rowCount(); ++row) {
        QModelIndex pickIdx = dm->index(row, G::PickColumn);
        // only picks
        if (pickIdx.data(Qt::EditRole).toString() == "true") {
            QModelIndex idx = dm->index(row, 0);
            // only filtered
            if (dm->sf->mapFromSource(idx).isValid()) {
                // if raw+jpg files have been combined
                if (combineRawJpg) {
                    // append the jpg if combineRawJpg and include combined jpgs
                    if (inclDupJpg && idx.data(G::DupIsJpgRole).toBool()) {
                        fPath = idx.data(G::PathRole).toString();
                        QFileInfo fileInfo(fPath);
                        pickList.append(fileInfo);
//                        qDebug() << __FUNCTION__ << "appending" << fPath;
                    }
                    // append combined raw file
                    if (idx.data(G::DupIsJpgRole).toBool()) {
                        idx = qvariant_cast<QModelIndex>(dm->index(row, 0).data(G::DupOtherIdxRole));
                    }
                }
                fPath = idx.data(G::PathRole).toString();
                QFileInfo fileInfo(fPath);
                pickList.append(fileInfo);
//                qDebug() << __FUNCTION__ << "appending" << fPath;
            }
        }
    }

    // stats
    fileCount = pickList.count();
    fileMB = 0;

    qulonglong bytes = 0;
    foreach(QFileInfo info, pickList) bytes += static_cast<qulonglong>(info.size());
    picksMB = bytes / 1024 / 1024;
    QString s1 = QString::number(fileCount);
    QString s2 = fileCount == 1 ? " file " : " files ";
    QString s3 = Utilities::formatMemory(bytes);
    QString s4 = "";
    if (inclDupJpg) s4 = " including duplicate jpg";
    ui->statsLabel->setText(s1 + s2 + s3 + s4);
    ui->gbsLabel->setText("");
}

void IngestDlg::quitIfNotEnoughSpace()
{
    if (G::isLogger) G::log(__FUNCTION__);

    // check if room on destination drives for images
    bool isFull = picksMB > availableMB;
    bool isFull2 = picksMB > availableMB2 && isBackup;
    if (isFull || isFull2) {
        QString s;
        if (isFull) s = drive + " does ";
        if (isFull2) s = drive2;
        if (isFull && isFull2) s = drive + " and " + drive2 + " do ";
        QString title = "Insufficient drive space";
        QString msg =
            s + "not have sufficient space available to copy the ingesting images.  No images "
            "will be ingested.  Please resolve the disk space issue and try again.";
        //        QMessageBox::warning(this, title, msg, QMessageBox::Ok);

        QMessageBox msgBox;
        int msgBoxWidth = 300;
        msgBox.setWindowTitle("Insufficient Drive Space");
        msgBox.setText(msg);
//        msgBox.setInformativeText("Do you want continue?");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStyleSheet(css);
        QSpacerItem* horizontalSpacer = new QSpacerItem(msgBoxWidth, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        QGridLayout* layout = static_cast<QGridLayout*>(msgBox.layout());
        layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
        msgBox.exec();
        // quit ingest dialog
        reject();
    }
}

void IngestDlg::ingest()
{
/*
    The files in pickList are renamed and ingested (copied) from the source folder to the
    destination folder.

    The destination folder path is appended to the ingest folder history (in the file menu).

    The description, if not "" is appended to the description completer list to facilitate
    future ingests.

    Each picked image is copied from the source to the destination.

    The destination file base name is based on the file name template selected. ie
    YYYY-MM-DD_xxxx would rename the source file test.jpg to 2019-01-20_0001.jpg if the seqNum
    = 1. The seqNum is incremented with each image in the pickList. This does not happen if
    the file name template does not include the sequence numbering ie use original file.

    If the destination folder already has a file with the same name then _# is appended to the
    destination base file name.

    If there is edited metadata it is written to a sidecar xmp file.

    Finally the source file is copied to the renamed destination.
*/

    quitIfNotEnoughSpace();

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
    isBackup ? n = 2 : n = 1;

    // copy picked images
    ui->progressBar->setVisible(true);
    seqNum =  ui->spinBoxStartNumber->value();
    QElapsedTimer t;
    qint64 bytesWritten = 0;
    t.restart();
    for (int i = 0; i < pickList.size(); ++i) {
        qint64 fileBytesToWrite = 0;
//        int progress = (i + 1) * 100 * n / (pickList.size() + 1);
        int progress = (i + 1) * 100 * n / (pickList.size());
//        qDebug() << __FUNCTION__
//                 << "progress =" << progress
//                 << "ui->progressBar->minimum() = " << ui->progressBar->minimum()
//                 << "ui->progressBar->maximum() = " << ui->progressBar->maximum();
        ui->progressBar->setValue(progress);
        qApp->processEvents();
        QFileInfo fileInfo = pickList.at(i);
        QString sourcePath = fileInfo.absoluteFilePath();
//        fileBytesToCopy += fileInfo.size();

        // seqNum is required by parseTokenString
        // increase sequence unless dup (raw + jpg)
        if (i > 0 && pickList.at(i).baseName() != pickList.at(i-1).baseName())
            seqNum++;

        // rename destination file based on the file naming template
        QString destBaseName =  parseTokenString(pickList.at(i), tokenString);
        QString suffix = fileInfo.suffix().toLower();
        QString dotSuffix = "." + suffix;
        QString destFileName = destBaseName + dotSuffix;
        QString sidecarName = destBaseName + ".xmp";

        // buffer to hold file with edited xmp data
        QByteArray buffer;

        // check if image already exists at destination folder
        QString destinationPath = folderPath + destFileName;
        QString backupPath = folderPath2 + destFileName;
        QString sidecarPath = folderPath + sidecarName;
        QString sidecarBackupPath = folderPath2 + sidecarName;

        // rename destination and fileName if already exists
        renameIfExists(destinationPath, destBaseName, dotSuffix);

        /* If there is edited xmp data in an eligible file format, copy it into a buffer. This
        routine is also used in MW::runExternalApp().  If combinedRawJpg then use the jpg from
        the datamodel as that is where changes to ratings and labels etc will have occurred.
        */

        // set the metadataChangedSourcePath depending on combineRawJpg
        QString metadataChangedSourcePath = sourcePath;
        if (combineRawJpg) {
            // only raw files included if combineRawJpg and there is a rawjpg pair
            int dmRow = dm->fPathRow[sourcePath];
            QModelIndex idx = dm->index(dmRow, 0);
            if (idx.isValid()) {
                // check if raw/jpg pair
                if (idx.data(G::DupHideRawRole).toBool()) {
                    QModelIndex jpgIdx = idx.data(G::DupOtherIdxRole).toModelIndex();
                    if (jpgIdx.isValid()) {
                        metadataChangedSourcePath = jpgIdx.data(G::PathRole).toString();
                    }
                }
            }
        }

        // copy source image to destination
        fileBytesToWrite += fileInfo.size();
        bool copyOk = QFile::copy(sourcePath, destinationPath);
        /* for demonstration:
        failedToCopy << sourcePath + " to " + destinationPath;
        integrityFailure << sourcePath + " not same as " + destinationPath;
        // */
        if (!copyOk) {
            qDebug() << __FUNCTION__ << "Failed to copy" << sourcePath << "to" << destinationPath;
            failedToCopy << sourcePath + " to " + destinationPath;
        }
        if (copyOk && integrityCheck) {
            if (!Utilities::integrityCheck(sourcePath, destinationPath)) {
                qDebug() << __FUNCTION__ << "Integrity failure" << sourcePath << "not same as" << destinationPath;
                integrityFailure << sourcePath + " not same as " + destinationPath;
            }
        }

        // copy source image to backup
        if(isBackup) {
            fileBytesToWrite += fileInfo.size();
            bool backupCopyOk = QFile::copy(sourcePath, backupPath);
            if (!backupCopyOk) {
                qDebug() << __FUNCTION__ << "Failed to copy" << sourcePath << "to" << backupPath;
                failedToCopy << sourcePath + " to " + backupPath;
            }
            if (backupCopyOk && integrityCheck) {
                if (!Utilities::integrityCheck(sourcePath, backupPath)) {
                    qDebug() << __FUNCTION__ << "Integrity failure" << sourcePath << "not same as" << backupPath;
                    integrityFailure << sourcePath + " not same as " + backupPath;
                }
            }
        }

        // write the sidecar xmp file
        ImageMetadata m = dm->imMetadata(metadataChangedSourcePath);
        bool okToWriteSidecar = ingestIncludeXmpSidecar &&
                                copyOk &&
                                metadata->writeMetadata(sourcePath, m, buffer);
        if (okToWriteSidecar) {
            QFile sidecarFile(sidecarPath);
            fileBytesToWrite += sidecarFile.size();
            sidecarFile.open(QIODevice::WriteOnly);
            qint64 bytesWritten = sidecarFile.write(buffer);
            if (bytesWritten == 0) failedToCopy << sidecarPath;
            sidecarFile.close();
            if (isBackup) {
                QFile sidecarFile(sidecarBackupPath);
                fileBytesToWrite += sidecarFile.size();
                sidecarFile.open(QIODevice::WriteOnly);
                qint64 bytesWritten = sidecarFile.write(buffer);
                if (bytesWritten == 0) failedToCopy << sidecarBackupPath;
                sidecarFile.close();
            }
        }
        // write to internal xmp (future enhancement?)
        /*
        if (ingestIncludeXmpSidecar && copyOk && metadata->writeMetadata(sourcePath, m, buffer)) {
            if (metadata->internalXmpFormats.contains(suffix)) {
                // write xmp data into image file       rgh needs some work!!!
                QFile newFile(destinationPath);
                fileBytesToCopy += newFile.size();
                newFile.open(QIODevice::WriteOnly);
                qint64 bytesWritten = newFile.write(buffer);
                qDebug() << __FUNCTION__ << bytesWritten << buffer.length();
                newFile.close();
            }
        }
        */

        // stats
        bytesWritten += fileBytesToWrite;
        double MBPerSec = static_cast<double>(bytesWritten) / 1048.576 / static_cast<double>(t.elapsed());
        QString s = QString::number(MBPerSec, 'f', 1) + " MB/sec";
        ui->gbsLabel->setText(s);
    }
    /*
    qDebug() << __FUNCTION__
             << "bytesCopied =" << bytesCopied
             << "msec =" << t.elapsed()
                ;
                // */

    // update ingest count for Winnow session
    G::ingestCount += pickList.size();
    G::ingestLastDate = QDate::currentDate();

    // show any ingest errors
    if (failedToCopy.length() || integrityFailure.length()) {
        IngestErrors ingestErrors(failedToCopy, integrityFailure);
        ingestErrors.exec();
    }


    QDialog::accept();
}

bool IngestDlg::parametersOk()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    QString errStr;
    bool err = false;
//    bool backup = ui->backupChk->isChecked();

    if(isAuto) {
        if(rootFolderPath.length() == 0) {
            err = true;
            errStr = "The primary location root folder is undefined";
        }

        if(isBackup) {
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

        if(isBackup) {
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
    if (G::isLogger) G::log(__FUNCTION__); 
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
    if (G::isLogger) G::log(__FUNCTION__); 
    QString root = QStandardPaths::displayName(QStandardPaths::HomeLocation);
    QString path = ui->manualFolderLabel->text();
    if (path.right(1) == "/") path = path.left(path.length() - 1);
    if (!QDir(path).exists()) path = root;
    QString s;
    s = QFileDialog::getExistingDirectory
        (this, tr("Choose Ingest Primary Folder"), path,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (s.length() > 0) {
        if(s.right(1) != "/") s += "/";
        folderPath = s;
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
    if (G::isLogger) G::log(__FUNCTION__); 
    QString root = QStandardPaths::displayName(QStandardPaths::HomeLocation);
    QString path = ui->manualFolderLabel_2->text();
    if (path.right(1) == "/") path = path.left(path.length() - 1);
    if (!QDir(path).exists()) path = root;
    QString s;
    s = QFileDialog::getExistingDirectory
        (this, tr("Choose Ingest Backup Folder"), path,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (s.length() > 0) {
        if(s.right(1) != "/") s += "/";
        folderPath2 = s;
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
    if (G::isLogger) G::log(__FUNCTION__); 
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
    if (G::isLogger) G::log(__FUNCTION__); 
    QString root = QStandardPaths::displayName(QStandardPaths::HomeLocation);
    rootFolderPath2 = QFileDialog::getExistingDirectory
        (this, tr("Choose Root Folder for Backup Location"), root,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (rootFolderPath2.length() > 0) {
        if(rootFolderPath2.right(1) != "/") rootFolderPath2 += "/";
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
    if (G::isLogger) G::log(__FUNCTION__); 
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

QString IngestDlg::parseTokenString(QFileInfo info, QString tokenString)
{
    if (G::isLogger) G::log(__FUNCTION__); 
    QString fPath = info.absoluteFilePath();
    ImageMetadata m = dm->imMetadata(fPath);
    createdDate = m.createdDate;
//    qDebug() << __FUNCTION__ << fPath << createdDate;
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
                tokenResult = m.title;
            if (currentToken == "CREATOR")
                tokenResult = m.creator;
            if (currentToken == "COPYRIGHT")
                tokenResult = m.copyright;
            if (currentToken == "ORIGINAL FILENAME")
                tokenResult = info.baseName();
            if (currentToken == "MAKE")
                tokenResult = m.make;
            if (currentToken == "MODEL")
                tokenResult = m.model;
            if (currentToken == "DIMENSIONS")
                tokenResult = m.dimensions;
            if (currentToken == "SHUTTER SPEED")
                tokenResult = m.exposureTime;
            if (currentToken == "APERTURE")
                tokenResult = m.aperture;
            if (currentToken == "ISO")
                tokenResult = m.ISO;
            if (currentToken == "FOCAL LENGTH")
                tokenResult = m.focalLength;
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
    if (G::isLogger) G::log(__FUNCTION__); 
    if (ui->autoRadio->isChecked() || isInitializing) {
        baseFolderDescription = (ui->descriptionLineEdit->text().length() > 0)
                ? ui->descriptionLineEdit->text() : "";
        /*
        qDebug() << __FUNCTION__
                 << "rootFolderPath =" << rootFolderPath
                 << "fromRootToBaseFolder =" << fromRootToBaseFolder
                 << "baseFolderDescription =" << baseFolderDescription;
                 // */
        folderPath = rootFolderPath + fromRootToBaseFolder + baseFolderDescription + "/";

        ui->folderLabel->setText(folderPath);
        ui->folderLabel->setToolTip(ui->folderLabel->text());

        baseFolderDescription2 = (ui->descriptionLineEdit_2->text().length() > 0)
                ? ui->descriptionLineEdit_2->text() : "";
        folderPath2 = rootFolderPath2 + fromRootToBaseFolder2 + baseFolderDescription2 + "/";
        ui->folderLabel_2->setText(folderPath2);
        ui->folderLabel_2->setToolTip(ui->folderLabel_2->text());
    }
    else {
        folderPath = ui->manualFolderLabel->text() + "/";
        folderPath2 = ui->manualFolderLabel_2->text() + "/";
    }
    updateExistingSequence();
    getAvailableStorageMB();
}

void IngestDlg::buildFileNameSequence()
{
/*
    This function displays what the ingested file paths will be for the 1st, 2nd and last
    file. If the primary location tab is selected the paths to the primary location are shown.
    If the backup location tab is chosen then the backup location paths are shown.
*/
    if (G::isLogger) G::log(__FUNCTION__); 
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
    // use auto
    if (ui->autoRadio->isChecked()) {
        // folderpath based on primary or backup tab selected
        if(ui->autoIngestTab->tabBar()->currentIndex() == 0) dirPath = folderPath;
        else dirPath = folderPath2;
    }
    else {
        dirPath = folderPath;
    }

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
    if (G::isLogger) G::log(__FUNCTION__); 
    static QString prevText = "";
    updateFolderPath();
    // copy primary description to backup description unless it is already different
    QString desc2 = ui->descriptionLineEdit_2->text();
    if (desc2  == prevText) ui->descriptionLineEdit_2->setText(arg1);

    prevText = arg1;
    ui->autoRadio->setChecked(true);
    updateEnabledState();
}

void IngestDlg::on_descriptionLineEdit_2_textChanged(const QString/* &arg1*/)
{
    if (G::isLogger) G::log(__FUNCTION__); 
    updateFolderPath();
//    ui->folderLabel_2->setText(arg1);
}

void IngestDlg::on_spinBoxStartNumber_valueChanged(const QString /* &arg1 */)
{
    if (G::isLogger) G::log(__FUNCTION__); 
    // updateFolderPath();
//    int sequenceNum = getSequenceStart(folderPath) + G::ingestCount;
//    if(ui->spinBoxStartNumber->value() < sequenceNum)
//        ui->spinBoxStartNumber->setValue(sequenceNum + 1);
    buildFileNameSequence();
}

int IngestDlg::getSequenceStart(const QString &path)
{
    if (G::isLogger) G::log(__FUNCTION__); 
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
    if (sequence < G::ingestCount) sequence = G::ingestCount;
    if (sequence > G::ingestCount) G::ingestCount = sequence;
    return sequence;
}

void IngestDlg::getAvailableStorageMB()
{
/*
    Return how many MB available on the drive pointed that contains path.
*/
    QStorageInfo info(rootFolderPath);
    if (info.isValid()) {
        drive = info.displayName();
        QString name = info.rootPath();
        qint64 bytes = info.bytesAvailable();
        availableMB = bytes / 1024 / 1024;
        QString s = name + " (" + drive + ") " + Utilities::formatMemory(bytes);
        if (picksMB > availableMB) ui->drive->setStyleSheet("QLabel {color:red;}");
        ui->drive->setText(s);
    }

    // backup drive
    info.setPath(rootFolderPath2);
    if (info.isValid() && isBackup) {
        drive2 = info.displayName();
        QString name = info.rootPath();
        qint64 bytes = info.bytesAvailable();
        availableMB2 = bytes / 1024 / 1024;
        QString s = name + " (" + drive2 + ") " + " " + Utilities::formatMemory(bytes);
        if (picksMB > availableMB2) ui->drive_2->setStyleSheet("QLabel {color:red;}");
        ui->drive_2->setText(s);
    }
    ui->drive_2->setVisible(isBackup);
}

void IngestDlg::updateEnabledState()
{
    if (G::isLogger) G::log(__FUNCTION__); 
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
//        isBackupChkBox->setEnabled(true);

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
//        isBackupChkBox->setEnabled(false);

        ui->selectFolderBtn->setEnabled(true);
        ui->selectFolderBtn_2->setEnabled(true);
        ui->manualFolderLabel->setEnabled(true);
        ui->manualFolderLabel_2->setEnabled(true);
    }
}

void IngestDlg::on_autoRadio_toggled(bool checked)
{
    if (G::isLogger) G::log(__FUNCTION__); 
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

void IngestDlg::on_manualRadio_toggled(bool /*checked*/)
{
    if (G::isLogger) G::log(__FUNCTION__); 
    isAuto = false;
    if (ui->manualFolderLabel->text().length() > 0) {
        folderPath = ui->manualFolderLabel->text();
        folderPath2 = ui->manualFolderLabel_2->text();
        updateEnabledState();
        buildFileNameSequence();
        updateExistingSequence();
        updateEnabledState();
        getAvailableStorageMB();
    }
}


void IngestDlg::initTokenList()
{
/*
The list of tokens in the token editor will appear in this order.
*/
    if (G::isLogger) G::log(__FUNCTION__); 
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
            << "MILLISECOND"
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
    if (G::isLogger) G::log(__FUNCTION__); 
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
    exampleMap["MILLISECOND"] = "167";
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
    if (G::isLogger) G::log(__FUNCTION__); 
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
    if (G::isLogger) G::log(__FUNCTION__); 
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
    if (G::isLogger) G::log(__FUNCTION__); 
    if (arg1 == "") return;
    QString tokenString = filenameTemplatesMap[arg1];
    if (!isInitializing) filenameTemplateSelected = ui->filenameTemplatesCB->currentIndex();
    updateExistingSequence();
}

void IngestDlg::on_pathTemplatesBtn_clicked()
{
/*

*/
    if (G::isLogger) G::log(__FUNCTION__); 
    // setup TokenDlg
    QMap<QString,QString> usingTokenMap;
    QString title = "Token Editor - Path from Root to Destination Folder";
    int index = ui->pathTemplatesCB->currentIndex();
    QString currentKey = ui->pathTemplatesCB->currentText();
    TokenDlg *tokenDlg = new TokenDlg(tokens, exampleMap, pathTemplatesMap, usingTokenMap,
                                      index, currentKey, title, this);

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
    if (G::isLogger) G::log(__FUNCTION__); 
    on_pathTemplatesBtn_clicked();
}

void IngestDlg::on_filenameTemplatesBtn_clicked()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    // setup TokenDlg
    // title is also used to filter warnings, so if you change it here also change
    // it in TokenDlg::updateUniqueFileNameWarning
    QString title = "Token Editor - Destination File Name";
    QMap<QString,QString> usingTokenMap;    // dummy
    int index = ui->filenameTemplatesCB->currentIndex();
    QString currentKey = ui->filenameTemplatesCB->currentText();
    TokenDlg *tokenDlg = new TokenDlg(tokens, exampleMap, filenameTemplatesMap, usingTokenMap,
                                      index, currentKey, title, this);
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
    if (G::isLogger) G::log(__FUNCTION__); 
    getPicks();
}

void IngestDlg::on_ejectChk_stateChanged(int /*arg1*/)
{
    if (G::isLogger) G::log(__FUNCTION__); 
    autoEjectUsb = ui->ejectChk->isChecked();
}

void IngestDlg::on_integrityChk_stateChanged(int /*arg1*/)
{
    if (G::isLogger) G::log(__FUNCTION__);
    integrityCheck = ui->integrityChk->isChecked();
}

void IngestDlg::on_includeXmpChk_stateChanged(int /*arg1*/)
{
    if (G::isLogger) G::log(__FUNCTION__);
    ingestIncludeXmpSidecar = ui->includeXmpChk->isChecked();
}

void IngestDlg::on_backupChk_stateChanged(int arg1)
{
    if (G::isLogger) G::log(__FUNCTION__); 
    isBackup = arg1;
    getAvailableStorageMB();
}

void IngestDlg::on_isBackupChkBox_stateChanged(int arg1)
{
    if (G::isLogger) G::log(__FUNCTION__); 
    isBackup = arg1;
    qDebug() << __FUNCTION__ << isBackup;
    ui->drive_2->setVisible(true);
}

void IngestDlg::on_helpBtn_clicked()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    QFile f(":/Docs/ingestautopath.html");
    f.open(QIODevice::ReadOnly);
    QDialog *dlg = new QDialog;
    QTextBrowser *text = new QTextBrowser;
//    QString style = "background-color: rgb(55,55,55); font-size:" + G::fontSize + "px;";
//    text->setStyleSheet(style);
    text->setMinimumWidth(600);
    text->setMinimumHeight(600);
    text->setContentsMargins(9,9,9,9);
    text->setHtml(f.readAll());
    text->setStyleSheet(css);
    dlg->setLayout(new QHBoxLayout);
    dlg->layout()->setContentsMargins(0,0,0,0);
    dlg->layout()->addWidget(text);
    dlg->exec();
}

void IngestDlg::on_cancelBtn_clicked()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    reject();
}

void IngestDlg::on_okBtn_clicked()
{
    if (G::isLogger) G::log(__FUNCTION__); 
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

void IngestDlg::on_autoIngestTab_currentChanged(int /*index*/)
{
    if (G::isLogger) G::log(__FUNCTION__); 
    buildFileNameSequence();
}

void IngestDlg::on_openIngestFolderChk_stateChanged(int arg1)
{
    if (G::isLogger) G::log(__FUNCTION__); 
     gotoIngestFolder = arg1;
}

void IngestDlg::fontSize()
{
/*
    The font size is determined by the assigned size G::fontsize in combination with the
    display screen viewing percentage.  This function is called by the resize and move
    events in case the user drags the dialog from one screen to another with different
    settings.
*/
    QScreen *screen = qApp->screenAt(geometry().center());
    if (screen == nullptr) return;
    qreal screenScaling = screen->logicalDotsPerInch() / 96;
    int fontPtSize = static_cast<int>(G::fontSize.toInt() / screenScaling);
    int fontPxSize = static_cast<int>(fontPtSize * G::ptToPx);
    QString titleShift = QString::number(fontPxSize * screenScaling / 2);

    QString fs = "QWidget {font-size: " + G::fontSize + "pt;}";
//    QString fs = "QWidget {font-size: " + QString::number(fontPtSize) + "pt;}";
    /*
    qDebug() << __FUNCTION__
             << "G::fontSize" << G::fontSize
             << "screenScaling" << screenScaling
             << "adjustedFontPtSize" << adjustedFontPtSize
             << "fs" << fs;  */

    QString titlePlacement =
         "QGroupBox {margin-top: " + titleShift + "px;}"
         "QGroupBox::title {top: -"  + titleShift + "px;}";

    setStyleSheet(fs + titlePlacement);
    ui->helpBtn->setStyleSheet(fs);
    ui->autoRadio->setStyleSheet(fs);

    ui->autoIngestTab->setStyleSheet(fs);
    ui->descriptionLineEdit->setStyleSheet(fs);
    ui->descriptionLineEdit_2->setStyleSheet(fs);
    ui->destinationFolderLabel->setStyleSheet(fs);
    ui->destinationFolderLabel_2->setStyleSheet(fs);
    ui->folderDescription->setStyleSheet(fs);
    ui->folderDescription_2->setStyleSheet(fs);
    ui->folderLabel->setStyleSheet(fs);
    ui->folderLabel_2->setStyleSheet(fs);
    ui->pathTemplatesCB->setStyleSheet(fs);
    ui->pathTemplatesCB_2->setStyleSheet(fs);
    ui->pathTemplatesBtn->setStyleSheet(fs);
    ui->pathTemplatesBtn_2->setStyleSheet(fs);
    ui->rootFolderLabel->setStyleSheet(fs);
    ui->rootFolderLabel_2->setStyleSheet(fs);
    ui->selectRootFolderBtn->setStyleSheet(fs);
    ui->selectRootFolderBtn_2->setStyleSheet(fs);
    ui->templateLabel1->setStyleSheet(fs);
    ui->templateLabel1_2->setStyleSheet(fs);

    ui->manualRadio->setStyleSheet(fs);
    ui->manualFolderLabel->setStyleSheet(fs);
    ui->manualFolderLabel_2->setStyleSheet(fs);
    ui->selectFolderBtn->setStyleSheet(fs);
    ui->selectFolderBtn_2->setStyleSheet(fs);

    ui->filenameGroupBox->setStyleSheet(fs);
    ui->templateLabel2->setStyleSheet(fs);
    ui->existingSequenceLabel->setStyleSheet(fs);
    ui->filenameTemplatesBtn->setStyleSheet(fs);
    ui->filenameTemplatesCB->setStyleSheet(fs);
    ui->spinBoxStartNumber->setStyleSheet(fs);
    ui->startSeqLabel->setStyleSheet(fs);

    ui->seqGroupBox->setStyleSheet(fs);
    ui->folderPathLabel->setStyleSheet(fs);
    ui->folderPathLabel_2->setStyleSheet(fs);
    ui->folderPathLabel_3->setStyleSheet(fs);
    ui->folderPathLabel_4->setStyleSheet(fs);

    ui->cancelBtn->setStyleSheet(fs);
    ui->okBtn->setStyleSheet(fs);

    adjustSize();
}

void IngestDlg::resizeEvent(QResizeEvent *event)
{
    fontSize();
    QDialog::resizeEvent(event);
}

void IngestDlg::moveEvent(QMoveEvent *event)
{
    fontSize();
    QDialog::moveEvent(event);
}

